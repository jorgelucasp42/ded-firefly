#include "firefly.h"
#include "utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t total_vars(const DedData *data) {
    return (size_t)data->n_units * (size_t)data->n_hours;
}

#pragma acc routine seq
static double clamp_double(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

#pragma acc routine seq
static void repair_flat_kernel(double *x,
                                const UnitData *units, const double *load,
                                int n_units, int n_hours) {
    for (int h = 0; h < n_hours; ++h) {
        for (int i = 0; i < n_units; ++i) {
            const UnitData *u = &units[i];
            size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
            double prev = (h == 0) ? u->P0 : x[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
            double lower = fmax(u->Pmin, prev - u->DR);
            double upper = fmin(u->Pmax, prev + u->UR);
            x[idx] = clamp_double(x[idx], lower, upper);
        }

        for (int iter = 0; iter < 50; ++iter) {
            double sum = 0.0;
            for (int i = 0; i < n_units; ++i) {
                size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
                sum += x[idx];
            }

            double diff = load[h] - sum;
            if (fabs(diff) < 1e-6) break;

            double total_room = 0.0;
            for (int i = 0; i < n_units; ++i) {
                const UnitData *u = &units[i];
                size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
                double prev = (h == 0) ? u->P0 : x[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
                double lower = fmax(u->Pmin, prev - u->DR);
                double upper = fmin(u->Pmax, prev + u->UR);

                if (diff > 0.0) {
                    total_room += (upper - x[idx]);
                } else {
                    total_room += (x[idx] - lower);
                }
            }

            if (total_room <= 1e-9) break;

            for (int i = 0; i < n_units; ++i) {
                const UnitData *u = &units[i];
                size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
                double prev = (h == 0) ? u->P0 : x[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
                double lower = fmax(u->Pmin, prev - u->DR);
                double upper = fmin(u->Pmax, prev + u->UR);

                double room = (diff > 0.0) ? (upper - x[idx]) : (x[idx] - lower);
                if (room <= 0.0) continue;

                double delta = (room / total_room) * diff;
                x[idx] = clamp_double(x[idx] + delta, lower, upper);
            }
        }
    }
}

static double cost_flat(const double *x, const DedData *data) {
    double total = 0.0;
    int n_hours = data->n_hours;

    for (int h = 0; h < n_hours; ++h) {
        double sum_p = 0.0;
        for (int i = 0; i < data->n_units; ++i) {
            const UnitData *u = &data->units[i];
            size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
            double p = x[idx];

            double fuel = u->a * p * p + u->b * p + u->c + fabs(u->e * sin(u->f * (u->Pmin - p)));
            total += fuel;
            sum_p += p;
        }

        double diff = sum_p - data->load[h];
        if (fabs(diff) > 1e-3) {
            total += 1e6 * diff * diff;
        }
    }

    return total;
}

static void init_population_flat(double *pop, const DedData *data, int n, unsigned seed) {
    size_t n_vars = total_vars(data);

    for (int i = 0; i < n; ++i) {
        Solution *tmp = solution_create(data);
        if (!tmp) continue;

        solution_init_random(tmp, data, seed + (unsigned)(i * 97u));
        memcpy(&pop[(size_t)i * n_vars], tmp->P, n_vars * sizeof(double));
        solution_destroy(tmp);
    }
}

static double run_firefly_core(const DedData *data,
                               const FireflyParams *params,
                               Solution *best,
                               int use_openmp) {
    (void)use_openmp;

    int n = params->n_fireflies;
    size_t n_vars = total_vars(data);
    size_t pop_size = (size_t)n * n_vars;

    double *pop_old  = (double *)calloc(pop_size, sizeof(double));
    double *pop_new  = (double *)calloc(pop_size, sizeof(double));
    double *cost_old = (double *)calloc((size_t)n, sizeof(double));
    double *cost_new = (double *)calloc((size_t)n, sizeof(double));
    if (!pop_old || !pop_new || !cost_old || !cost_new) {
        free(pop_old); free(pop_new);
        free(cost_old); free(cost_new);
        return 1e30;
    }

    Solution *trial = solution_create(data);
    if (!trial) {
        free(pop_old); free(pop_new);
        free(cost_old); free(cost_new);
        return 1e30;
    }

    init_population_flat(pop_old, data, n, params->seed);

    double best_cost = 1e30;
    for (int i = 0; i < n; ++i) {
        double c = cost_flat(&pop_old[(size_t)i * n_vars], data);
        cost_old[i] = c;
        if (c < best_cost) {
            best_cost = c;
            memcpy(best->P, &pop_old[(size_t)i * n_vars], n_vars * sizeof(double));
        }
    }

    Rng base;
    rng_seed(&base, params->seed);

    const UnitData *units = data->units;
    const double *load = data->load;

    /* Acumuladores de tempo — mesmos buckets que bin_acc para comparação direta:
       t_move  = CPU puro (trial, atualização de best, cópias)
       t_pcie  = todas as transferências PCIe (acc update device/self)
       t_kernel = todos os kernels OpenACC (movimento, repair, custo) */
    double t_move = 0.0, t_pcie = 0.0, t_kernel = 0.0;

#pragma acc data copyin(units[0:data->n_units], load[0:data->n_hours]) \
                create(pop_old[0:pop_size], pop_new[0:pop_size],       \
                       cost_old[0:n], cost_new[0:n])
    {
#pragma acc update device(pop_old[0:pop_size], cost_old[0:n])

        for (int iter = 0; iter < params->max_iters; ++iter) {
            double alpha_iter = params->alpha * (1.0 - (double)iter / params->max_iters);
            if (alpha_iter < 1e-4) alpha_iter = 1e-4;
            double t0;

            /* ── (t_kernel) Kernel: Movimento ────────────────────── */
            /* RNG inline (LCG) por vagalume, mesma semente que firefly_seq/omp:
               seed + iter*1315423911 + i*2654435761.
               r² de pop_new[i] (posição já modificada pelos j anteriores),
               equivalente ao comportamento de firefly_seq.c / firefly_omp.c. */
            t0 = now_sec();
#pragma acc parallel loop present(pop_old[0:pop_size], pop_new[0:pop_size], \
                                   cost_old[0:n], units[0:data->n_units])
            for (int i = 0; i < n; ++i) {
                size_t base_i = (size_t)i * n_vars;
                for (size_t k = 0; k < n_vars; ++k) {
                    pop_new[base_i + k] = pop_old[base_i + k];
                }

                unsigned rng_s = params->seed +
                                  (unsigned)((unsigned)iter * 1315423911u +
                                             (unsigned)i   * 2654435761u);
                if (rng_s == 0u) rng_s = 1u;

                for (int j = 0; j < n; ++j) {
                    if (cost_old[j] < cost_old[i]) {
                        size_t base_j = (size_t)j * n_vars;

                        double r2 = 0.0;
                        for (size_t k = 0; k < n_vars; ++k) {
                            double d = pop_new[base_i + k] - pop_old[base_j + k];
                            r2 += d * d;
                        }
                        double beta = params->beta0 * exp(-params->gamma * r2);

                        for (size_t k = 0; k < n_vars; ++k) {
                            int unit_idx = (int)(k / (size_t)data->n_hours);
                            const UnitData u = units[unit_idx];
                            double span = u.Pmax - u.Pmin;
                            if (span <= 0.0) span = 1.0;

                            rng_s = 1664525u * rng_s + 1013904223u;
                            double eps_raw = (double)((rng_s >> 8) & 0x00FFFFFFu)
                                           / (double)0x01000000u;
                            double eps = alpha_iter * (eps_raw - 0.5) * span;
                            pop_new[base_i + k] += beta * (pop_old[base_j + k] -
                                                           pop_new[base_i + k]) + eps;
                        }
                    }
                }
            }
            t_kernel += now_sec() - t0;

            /* ── (t_kernel) Kernel: Repair ───────────────────────── */
            t0 = now_sec();
#pragma acc parallel loop present(pop_new[0:pop_size], \
                                   units[0:data->n_units], load[0:data->n_hours])
            for (int i = 0; i < n; ++i) {
                repair_flat_kernel(&pop_new[(size_t)i * n_vars],
                                   units, load, data->n_units, data->n_hours);
            }
            t_kernel += now_sec() - t0;

            /* ── (t_move + t_pcie) Elite injection ───────────────── */
            t0 = now_sec();
            memcpy(&pop_new[0], best->P, n_vars * sizeof(double));
            t_move += now_sec() - t0;
            t0 = now_sec();
#pragma acc update device(pop_new[0:n_vars])
            t_pcie += now_sec() - t0;

            /* ── (t_move) Trial na CPU ────────────────────────────── */
            t0 = now_sec();
            for (int attempt = 0; attempt < 3; ++attempt) {
                memcpy(trial->P, best->P, n_vars * sizeof(double));

                int n_mut = 1 + (int)(rng_u32(&base) % 3);
                for (int m = 0; m < n_mut; ++m) {
                    size_t idx = (size_t)(rng_u32(&base) % n_vars);
                    int unit_idx = (int)(idx / (size_t)data->n_hours);
                    const UnitData *u = &data->units[unit_idx];
                    double span = u->Pmax - u->Pmin;
                    if (span <= 0.0) span = 1.0;
                    double delta = ((double)((rng_u32(&base) % 5) - 2)) * 0.01 * span;
                    trial->P[idx] += delta;
                }

                solution_repair(trial, data);
                double trial_cost = ded_cost(trial, data);
                if (trial_cost < best_cost) {
                    best_cost = trial_cost;
                    memcpy(best->P, trial->P, n_vars * sizeof(double));
                }
            }
            t_move += now_sec() - t0;

            /* ── (t_move + t_pcie) Re-injeta melhor ──────────────── */
            t0 = now_sec();
            memcpy(&pop_new[0], best->P, n_vars * sizeof(double));
            t_move += now_sec() - t0;
            t0 = now_sec();
#pragma acc update device(pop_new[0:n_vars])
            t_pcie += now_sec() - t0;

            /* ── (t_kernel) Kernel: Custo ────────────────────────── */
            t0 = now_sec();
#pragma acc parallel loop present(pop_new[0:pop_size], cost_new[0:n], \
                                   units[0:data->n_units], load[0:data->n_hours])
            for (int i = 0; i < n; ++i) {
                size_t base_i = (size_t)i * n_vars;
                double total = 0.0;

                for (int h = 0; h < data->n_hours; ++h) {
                    double sum_p = 0.0;
                    for (int u_idx = 0; u_idx < data->n_units; ++u_idx) {
                        UnitData u = units[u_idx];
                        size_t idx = base_i + (size_t)u_idx * (size_t)data->n_hours + (size_t)h;
                        double p = pop_new[idx];

                        double fuel = u.a * p * p + u.b * p + u.c
                                    + fabs(u.e * sin(u.f * (u.Pmin - p)));
                        total += fuel;
                        sum_p += p;
                    }

                    double diff = sum_p - load[h];
                    if (fabs(diff) > 1e-3) {
                        total += 1e6 * diff * diff;
                    }
                }

                cost_new[i] = total;
            }
            t_kernel += now_sec() - t0;

            /* ── (t_pcie) Download cost_new → host ───────────────── */
            t0 = now_sec();
#pragma acc update self(cost_new[0:n])
            t_pcie += now_sec() - t0;

            /* ── (t_move) Encontra melhor ─────────────────────────── */
            t0 = now_sec();
            int best_idx = -1;
            double min_cost = best_cost;
            for (int i = 0; i < n; ++i) {
                if (cost_new[i] < min_cost) {
                    min_cost = cost_new[i];
                    best_idx = i;
                }
            }
            t_move += now_sec() - t0;

            /* ── (t_pcie) Download seletivo do vagalume vencedor ─── */
            if (best_idx >= 0) {
                size_t base_best = (size_t)best_idx * n_vars;
                t0 = now_sec();
#pragma acc update self(pop_new[base_best:n_vars])
                t_pcie += now_sec() - t0;
                best_cost = min_cost;
                memcpy(best->P, &pop_new[base_best], n_vars * sizeof(double));
            }

            if ((iter + 1) % 500 == 0) {
                printf("iter=%d best_cost=%.10f\n", iter + 1, best_cost);
            }

            /* ── (t_move + t_pcie) Propaga cost_old → device ─────── */
            t0 = now_sec();
            for (int i = 0; i < n; ++i) {
                cost_old[i] = cost_new[i];
            }
            t_move += now_sec() - t0;
            t0 = now_sec();
#pragma acc update device(cost_old[0:n])
            t_pcie += now_sec() - t0;

            double *tmp_pop = pop_old;
            pop_old = pop_new;
            pop_new = tmp_pop;

            double *tmp_cost = cost_old;
            cost_old = cost_new;
            cost_new = tmp_cost;
        }

        /* ── Relatório de perfil de tempo ──────────────────────────────── */
        {
            double t_total = t_move + t_pcie + t_kernel;
            printf("\n=== Perfil de tempo — bin_acc_full (%d iters, %d vagalumes) ===\n",
                   params->max_iters, params->n_fireflies);
            printf("  (c) CPU (trial+best):         %8.3f s  (%5.1f%%)\n",
                   t_move,   t_total > 0.0 ? 100.0 * t_move   / t_total : 0.0);
            printf("  (b) Transferencias PCIe:      %8.3f s  (%5.1f%%)\n",
                   t_pcie,   t_total > 0.0 ? 100.0 * t_pcie   / t_total : 0.0);
            printf("  (a) Kernels GPU (mov+rep+custo): %5.3f s  (%5.1f%%)\n",
                   t_kernel, t_total > 0.0 ? 100.0 * t_kernel / t_total : 0.0);
            printf("  ---\n");
            printf("  Total loop medido:            %8.3f s\n", t_total);
            printf("  Speedup potencial (kernels vs total): %.2fx\n",
                   t_kernel > 0.0 ? t_total / t_kernel : 0.0);
            printf("=============================================================\n\n");
        }
        /* ────────────────────────────────────────────────────────────────── */

        solution_destroy(trial);
    }

    free(pop_old);
    free(pop_new);
    free(cost_old);
    free(cost_new);

    return best_cost;
}

double firefly_optimize(const DedData *data,
                        const FireflyParams *params,
                        Solution *best) {
    return run_firefly_core(data, params, best, 0);
}

double firefly_optimize_omp(const DedData *data,
                            const FireflyParams *params,
                            Solution *best) {
    return run_firefly_core(data, params, best, 1);
}

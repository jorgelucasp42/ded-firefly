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

/*
 * repair_flat_kernel: corrige restrições de rampa e balanço de carga.
 * O loop em h tem dependência real (h depende de h-1), então permanece seq.
 * O paralelismo é explorado no nível externo (loop sobre vagalumes = gangs).
 */
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

    int n        = params->n_fireflies;
    size_t n_vars   = total_vars(data);
    size_t pop_size = (size_t)n * n_vars;

    double *pop_old  = (double *)calloc(pop_size, sizeof(double));
    double *pop_new  = (double *)calloc(pop_size, sizeof(double));
    double *cost_old = (double *)calloc((size_t)n, sizeof(double));
    double *cost_new = (double *)calloc((size_t)n, sizeof(double));
    double *noise_buf = (double *)calloc(pop_size, sizeof(double));
    /*
     * beta_buf[i*n + j]: beta pré-computado para o par (i,j).
     * Armazenado na GPU entre os dois passes do kernel de movimento.
     * 0.0 significa "j não atrai i" (cost_old[j] >= cost_old[i]).
     */
    double *beta_buf = (double *)calloc((size_t)n * (size_t)n, sizeof(double));

    if (!pop_old || !pop_new || !cost_old || !cost_new || !noise_buf || !beta_buf) {
        free(pop_old); free(pop_new);
        free(cost_old); free(cost_new);
        free(noise_buf); free(beta_buf);
        return 1e30;
    }

    Solution *trial = solution_create(data);
    if (!trial) {
        free(pop_old); free(pop_new);
        free(cost_old); free(cost_new);
        free(noise_buf); free(beta_buf);
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
    const double   *load  = data->load;
    int n_units = data->n_units;
    int n_hours = data->n_hours;

    double t_move = 0.0, t_pcie = 0.0, t_kernel = 0.0;

#pragma acc data \
        copyin(units[0:n_units], load[0:n_hours])           \
        create(pop_old[0:pop_size], pop_new[0:pop_size],    \
               cost_old[0:n],      cost_new[0:n],           \
               noise_buf[0:pop_size], beta_buf[0:n*n])
    {
#pragma acc update device(pop_old[0:pop_size], cost_old[0:n])

        for (int iter = 0; iter < params->max_iters; ++iter) {

            double alpha_iter = params->alpha * (1.0 - (double)iter / params->max_iters);
            if (alpha_iter < 1e-4) alpha_iter = 1e-4;
            double t0;

            /* ── (t_move) Gera ruído uniforme na CPU ─────────────── */
            t0 = now_sec();
            for (size_t k = 0; k < pop_size; ++k)
                noise_buf[k] = rng_uniform01(&base);
            t_move += now_sec() - t0;

            /* ── (t_pcie) Upload noise → device ──────────────────── */
            t0 = now_sec();
#pragma acc update device(noise_buf[0:pop_size])
            t_pcie += now_sec() - t0;

            /* ════════════════════════════════════════════════════════
             * KERNEL 1 — Pré-computa beta[i][j] para todos os pares.
             *
             * Paralelismo: gang = i, vector = j
             * Cada thread (i,j) calcula r² (redução sobre k) e beta.
             * Sem atomics; cada thread escreve em beta_buf[i*n+j] exclusivo.
             * ════════════════════════════════════════════════════════ */
            t0 = now_sec();
#pragma acc parallel loop gang \
            present(pop_old[0:pop_size], cost_old[0:n], beta_buf[0:n*n])
            for (int i = 0; i < n; ++i) {
                size_t base_i = (size_t)i * n_vars;

#pragma acc loop vector
                for (int j = 0; j < n; ++j) {
                    if (cost_old[j] >= cost_old[i]) {
                        beta_buf[(size_t)i * (size_t)n + (size_t)j] = 0.0;
                        continue;
                    }
                    size_t base_j = (size_t)j * n_vars;
                    double r2 = 0.0;
                    for (size_t k = 0; k < n_vars; ++k) {
                        double d = pop_old[base_i + k] - pop_old[base_j + k];
                        r2 += d * d;
                    }
                    beta_buf[(size_t)i * (size_t)n + (size_t)j] =
                        params->beta0 * exp(-params->gamma * r2);
                }
            }

            /* ════════════════════════════════════════════════════════
             * KERNEL 2 — Aplica atrações + ruído → pop_new[i][k].
             *
             * Paralelismo: gang = i, vector = k
             * Cada thread (i,k) acumula contribuição de todos os j
             * de forma sequencial, mas SEM atomics:
             *   - pop_new[base_i + k] é escrito apenas pela thread k do gang i
             *   - loop j é seq dentro de cada (gang,vector) — mas o paralelismo
             *     real está em n×n_vars threads simultâneas, não em atomics.
             * ════════════════════════════════════════════════════════ */
#pragma acc parallel loop gang \
            present(pop_old[0:pop_size], pop_new[0:pop_size],   \
                    cost_old[0:n], beta_buf[0:n*n],             \
                    noise_buf[0:pop_size], units[0:n_units])
            for (int i = 0; i < n; ++i) {
                size_t base_i = (size_t)i * n_vars;

#pragma acc loop vector
                for (size_t k = 0; k < n_vars; ++k) {
                    double val = pop_old[base_i + k];

                    /* Acumula atrações de todos os j que brilham mais */
                    for (int j = 0; j < n; ++j) {
                        double beta = beta_buf[(size_t)i * (size_t)n + (size_t)j];
                        if (beta > 0.0) {
                            val += beta * (pop_old[(size_t)j * n_vars + k] - pop_old[base_i + k]);
                        }
                    }

                    /* Aplica ruído uma vez por (i, k) */
                    int unit_idx = (int)(k / (size_t)n_hours);
                    const UnitData u = units[unit_idx];
                    double span = u.Pmax - u.Pmin;
                    if (span <= 0.0) span = 1.0;
                    double eps = alpha_iter * (noise_buf[base_i + k] - 0.5) * span;
                    val += eps;

                    pop_new[base_i + k] = val;
                }
            }
            t_kernel += now_sec() - t0;

            /* ── (t_kernel) KERNEL 3 — Repair ───────────────────── */
            t0 = now_sec();
#pragma acc parallel loop gang \
            present(pop_new[0:pop_size], units[0:n_units], load[0:n_hours])
            for (int i = 0; i < n; ++i) {
                repair_flat_kernel(&pop_new[(size_t)i * n_vars],
                                   units, load, n_units, n_hours);
            }
            t_kernel += now_sec() - t0;

            /* ── Elite injection (host → device) ─────────────────── */
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
                    int unit_idx = (int)(idx / (size_t)n_hours);
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

            /* ── Re-injeta melhor após trial ─────────────────────── */
            t0 = now_sec();
            memcpy(&pop_new[0], best->P, n_vars * sizeof(double));
            t_move += now_sec() - t0;
            t0 = now_sec();
#pragma acc update device(pop_new[0:n_vars])
            t_pcie += now_sec() - t0;

            /* ── (t_kernel) KERNEL 4 — Custo ─────────────────────── */
            t0 = now_sec();
#pragma acc parallel loop gang \
            present(pop_new[0:pop_size], cost_new[0:n], \
                    units[0:n_units], load[0:n_hours])
            for (int i = 0; i < n; ++i) {
                size_t base_i = (size_t)i * n_vars;
                double total  = 0.0;

                for (int h = 0; h < n_hours; ++h) {
                    double sum_p = 0.0;
                    for (int u_idx = 0; u_idx < n_units; ++u_idx) {
                        UnitData u = units[u_idx];
                        size_t idx = base_i + (size_t)u_idx * (size_t)n_hours + (size_t)h;
                        double p = pop_new[idx];

                        double fuel = u.a * p * p + u.b * p + u.c
                                    + fabs(u.e * sin(u.f * (u.Pmin - p)));
                        total  += fuel;
                        sum_p  += p;
                    }
                    double diff = sum_p - load[h];
                    if (fabs(diff) > 1e-3)
                        total += 1e6 * diff * diff;
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

            if ((iter + 1) % 500 == 0)
                printf("iter=%d best_cost=%.10f\n", iter + 1, best_cost);

            /* ── Propaga cost_new → cost_old no device ────────────── */
            t0 = now_sec();
            for (int i = 0; i < n; ++i)
                cost_old[i] = cost_new[i];
            t_move += now_sec() - t0;
            t0 = now_sec();
#pragma acc update device(cost_old[0:n])
            t_pcie += now_sec() - t0;

            /* Troca ponteiros (pop e cost) */
            double *tmp_pop = pop_old; pop_old = pop_new; pop_new = tmp_pop;
            double *tmp_cost = cost_old; cost_old = cost_new; cost_new = tmp_cost;
        }

        /* ── Relatório de tempo ──────────────────────────────────── */
        {
            double t_total = t_move + t_pcie + t_kernel;
            printf("\n=== Perfil de tempo — bin_acc_sync (%d iters, %d vagalumes) ===\n",
                   params->max_iters, params->n_fireflies);
            printf("  (c) CPU (noise+trial+best):      %8.3f s  (%5.1f%%)\n",
                   t_move,   t_total > 0.0 ? 100.0 * t_move   / t_total : 0.0);
            printf("  (b) Transferencias PCIe:         %8.3f s  (%5.1f%%)\n",
                   t_pcie,   t_total > 0.0 ? 100.0 * t_pcie   / t_total : 0.0);
            printf("  (a) Kernels GPU (mov+rep+custo): %8.3f s  (%5.1f%%)\n",
                   t_kernel, t_total > 0.0 ? 100.0 * t_kernel / t_total : 0.0);
            printf("  ---\n");
            printf("  Total loop medido:             %8.3f s\n", t_total);
            printf("=============================================================\n\n");
        }

        solution_destroy(trial);
    }

    free(pop_old);
    free(pop_new);
    free(cost_old);
    free(cost_new);
    free(noise_buf);
    free(beta_buf);

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
#include "firefly.h"
#include "utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double solution_distance2(const Solution *a, const Solution *b, size_t n_vars) {
    double dist = 0.0;
    for (size_t k = 0; k < n_vars; ++k) {
        double d = a->P[k] - b->P[k];
        dist += d * d;
    }
    return dist;
}

/* Calcula custo de n soluções via GPU.
   Quando t_pcie/t_kernel são não-nulos, acumula:
     *t_pcie  += tempo de upload (flatP) + download (cost)
     *t_kernel += tempo do kernel OpenACC puro
   Passa NULL em ambos para a chamada de inicialização (sem overhead). */
static void compute_costs_acc(const DedData *data, Solution **pop, double *cost, int n, double *flatP,
                               double *t_pcie, double *t_kernel) {
    int n_units = data->n_units;
    int n_hours = data->n_hours;
    size_t n_vars = (size_t)n_units * (size_t)n_hours;
    size_t total_vars = (size_t)n * n_vars;

    const UnitData *units = data->units;
    const double *load = data->load;

    if (!flatP) {
        for (int i = 0; i < n; ++i) {
            cost[i] = ded_cost(pop[i], data);
        }
        return;
    }

    for (int i = 0; i < n; ++i) {
        memcpy(&flatP[(size_t)i * n_vars], pop[i]->P, n_vars * sizeof(double));
    }

    double t0 = 0.0;  /* inicializado para evitar warning de uso potencial sem init */

    /* ── (b) PCIe upload ── */
    if (t_pcie) t0 = now_sec();
#pragma acc update device(flatP[0:total_vars])
    if (t_pcie) *t_pcie += now_sec() - t0;

    /* ── (a) Kernel GPU ── */
    if (t_kernel) t0 = now_sec();
#pragma acc parallel loop present(flatP[0:total_vars], units[0:n_units], load[0:n_hours], cost[0:n])
    for (int s = 0; s < n; ++s) {
        double total = 0.0;
        size_t base = (size_t)s * n_vars;

        for (int h = 0; h < n_hours; ++h) {
            double sum_p = 0.0;

            for (int u_idx = 0; u_idx < n_units; ++u_idx) {
                UnitData u = units[u_idx];
                size_t idx = base + (size_t)u_idx * (size_t)n_hours + (size_t)h;
                double p = flatP[idx];

                double fuel =
                    u.a * p * p +
                    u.b * p +
                    u.c +
                    fabs(u.e * sin(u.f * (u.Pmin - p)));

                total += fuel;
                sum_p += p;
            }

            double diff = sum_p - load[h];
            if (fabs(diff) > 1e-3) {
                total += 1e6 * diff * diff;
            }
        }

        cost[s] = total;
    }
    /* #pragma acc parallel loop é síncrono: host aguarda o device antes de continuar */
    if (t_kernel) *t_kernel += now_sec() - t0;

    /* ── (b) PCIe download ── */
    if (t_pcie) t0 = now_sec();
#pragma acc update self(cost[0:n])
    if (t_pcie) *t_pcie += now_sec() - t0;
}

static void move_firefly(const DedData *data,
                         const FireflyParams *params,
                         Solution *xi,
                         const Solution *xj,
                         Rng *rng,
                         double alpha_scale) {
    size_t n_vars = (size_t)data->n_units * (size_t)data->n_hours;
    double r2 = solution_distance2(xi, xj, n_vars);
    double beta = params->beta0 * exp(-params->gamma * r2);

    for (int i = 0; i < data->n_units; ++i) {
        const UnitData *u = &data->units[i];
        double span = u->Pmax - u->Pmin;
        if (span <= 0.0) span = 1.0;

        for (int h = 0; h < data->n_hours; ++h) {
            size_t idx = (size_t)i * (size_t)data->n_hours + (size_t)h;
            double eps = alpha_scale * (rng_uniform01(rng) - 0.5) * span;
            xi->P[idx] += beta * (xj->P[idx] - xi->P[idx]) + eps;
        }
    }

    solution_repair(xi, data);
}

static double run_firefly_core(const DedData *data,
                               const FireflyParams *params,
                               Solution *best,
                               int use_openmp) {
    (void)use_openmp;
    int n = params->n_fireflies;
    size_t n_vars = (size_t)data->n_units * (size_t)data->n_hours;
    size_t total_vars = (size_t)n * n_vars;

    Solution **popA = (Solution **)calloc((size_t)n, sizeof(Solution *));
    Solution **popB = (Solution **)calloc((size_t)n, sizeof(Solution *));
    double *costA = (double *)calloc((size_t)n, sizeof(double));
    double *costB = (double *)calloc((size_t)n, sizeof(double));
    if (!popA || !popB || !costA || !costB) {
        free(popA);
        free(popB);
        free(costA);
        free(costB);
        return 1e30;
    }

    for (int i = 0; i < n; ++i) {
        popA[i] = solution_create(data);
        popB[i] = solution_create(data);
        if (!popA[i] || !popB[i]) {
            for (int k = 0; k <= i; ++k) {
                solution_destroy(popA[k]);
                solution_destroy(popB[k]);
            }
            free(popA);
            free(popB);
            free(costA);
            free(costB);
            return 1e30;
        }
    }

    double *flatP = (double *)malloc(total_vars * sizeof(double));
    if (!flatP) {
        for (int k = 0; k < n; ++k) {
            solution_destroy(popA[k]);
            solution_destroy(popB[k]);
        }
        free(popA);
        free(popB);
        free(costA);
        free(costB);
        return 1e30;
    }

    Rng base;
    rng_seed(&base, params->seed);

    double best_cost = 1e30;
    for (int i = 0; i < n; ++i) {
        solution_init_random(popA[i], data, rng_u32(&base));
    }

    const UnitData *units = data->units;
    const double *load = data->load;

    Solution *trial = solution_create(data);
    if (!trial) {
        for (int i = 0; i < n; ++i) {
            solution_destroy(popA[i]);
            solution_destroy(popB[i]);
        }
        free(popA);
        free(popB);
        free(costA);
        free(costB);
        free(flatP);
        return 1e30;
    }

    /* Acumuladores de tempo — medidos apenas dentro do loop principal */
    double t_move = 0.0, t_pcie = 0.0, t_kernel = 0.0;

#pragma acc data copyin(units[0:data->n_units], load[0:data->n_hours]) create(flatP[0:total_vars], costA[0:n], costB[0:n])
    {
        /* Custo inicial — não entra nos acumuladores (executa uma única vez) */
        compute_costs_acc(data, popA, costA, n, flatP, NULL, NULL); /* init: sem timing */
        for (int i = 0; i < n; ++i) {
            if (costA[i] < best_cost) {
                best_cost = costA[i];
                memcpy(best->P, popA[i]->P, n_vars * sizeof(double));
            }
        }

        Solution **pop_old = popA;
        Solution **pop_new = popB;
        double *cost_old = costA;
        double *cost_new = costB;

        for (int iter = 0; iter < params->max_iters; ++iter) {
        double alpha_iter = params->alpha * (1.0 - (double)iter / params->max_iters);
        if (alpha_iter < 1e-4) alpha_iter = 1e-4;

        /* ── (c) CPU: Movement + Repair + Elite injection + Trial ──── */
        double t0 = now_sec();

        for (int i = 0; i < n; ++i) {
            memcpy(pop_new[i]->P, pop_old[i]->P, n_vars * sizeof(double));

            Rng rng;
            rng_seed(&rng, params->seed +
                          (unsigned)(iter * 1315423911u +
                          (unsigned)i * 2654435761u));

            for (int j = 0; j < n; ++j) {
                if (cost_old[j] < cost_old[i]) {
                    move_firefly(data, params, pop_new[i], pop_old[j], &rng, alpha_iter);
                }
            }
        }

        memcpy(pop_new[0]->P, best->P, n_vars * sizeof(double));

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
        /* ───────────────────────────────────────────────────────────── */

        /* ── (b) PCIe + (a) Kernel GPU ─────────────────────────────── */
            compute_costs_acc(data, pop_new, cost_new, n, flatP, &t_pcie, &t_kernel);

            memcpy(pop_new[0]->P, best->P, n_vars * sizeof(double));
            cost_new[0] = best_cost;

            for (int i = 0; i < n; ++i) {
                if (cost_new[i] < best_cost) {
                    best_cost = cost_new[i];
                    memcpy(best->P, pop_new[i]->P, n_vars * sizeof(double));
                }
            }

            if ((iter + 1) % 500 == 0) {
                printf("iter=%d best_cost=%.10f\n", iter + 1, best_cost);
            }

            Solution **tmpP = pop_old;
            pop_old = pop_new;
            pop_new = tmpP;

            double *tmpC = cost_old;
            cost_old = cost_new;
            cost_new = tmpC;
        }

        /* ── Relatório de perfil de tempo ──────────────────────────────── */
        {
            double t_total = t_move + t_pcie + t_kernel;
            printf("\n=== Perfil de tempo — bin_acc (%d iters, %d vagalumes) ===\n",
                   params->max_iters, params->n_fireflies);
            printf("  (c) Movement + Repair (CPU):  %8.3f s  (%5.1f%%)\n",
                   t_move,   t_total > 0.0 ? 100.0 * t_move   / t_total : 0.0);
            printf("  (b) Transferencias PCIe:      %8.3f s  (%5.1f%%)\n",
                   t_pcie,   t_total > 0.0 ? 100.0 * t_pcie   / t_total : 0.0);
            printf("  (a) Kernel GPU (custo):       %8.3f s  (%5.1f%%)\n",
                   t_kernel, t_total > 0.0 ? 100.0 * t_kernel / t_total : 0.0);
            printf("  ---\n");
            printf("  Total loop medido:            %8.3f s\n", t_total);
            printf("  Speedup potencial (kernel only vs total): %.2fx\n",
                   t_kernel > 0.0 ? t_total / t_kernel : 0.0);
            printf("==========================================================\n\n");
        }
        /* ────────────────────────────────────────────────────────────────── */

        solution_destroy(trial);
    }

    for (int i = 0; i < n; ++i) {
        solution_destroy(popA[i]);
        solution_destroy(popB[i]);
    }
    free(popA);
    free(popB);
    free(costA);
    free(costB);
    free(flatP);

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

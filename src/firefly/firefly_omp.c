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

}

static double run_firefly_core(const DedData *data,
                               const FireflyParams *params,
                               Solution *best,
                               int use_openmp) {
    (void)use_openmp;
    int n = params->n_fireflies;
    size_t n_vars = (size_t)data->n_units * (size_t)data->n_hours;

    // double-buffer
    Solution **popA = (Solution **)calloc((size_t)n, sizeof(Solution *));
    Solution **popB = (Solution **)calloc((size_t)n, sizeof(Solution *));
    double *costA = (double *)calloc((size_t)n, sizeof(double));
    double *costB = (double *)calloc((size_t)n, sizeof(double));
    if (!popA || !popB || !costA || !costB) {
        free(popA); free(popB); free(costA); free(costB);
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
            free(popA); free(popB); free(costA); free(costB);
            return 1e30;
        }
    }

    // init
    Rng base;
    rng_seed(&base, params->seed);

    double best_cost = 1e30;
    for (int i = 0; i < n; ++i) {
        solution_init_random(popA[i], data, rng_u32(&base));
        costA[i] = ded_cost(popA[i], data);

        if (costA[i] < best_cost) {
            best_cost = costA[i];
            memcpy(best->P, popA[i]->P, n_vars * sizeof(double));
        }
    }

    Solution **pop_old = popA;
    Solution **pop_new = popB;
    double *cost_old = costA;
    double *cost_new = costB;
    Solution *trial = solution_create(data);
    if (!trial) {
        for (int i = 0; i < n; ++i) {
            solution_destroy(popA[i]);
            solution_destroy(popB[i]);
        }
        free(popA); free(popB);
        free(costA); free(costB);
        return 1e30;
    }

    for (int iter = 0; iter < params->max_iters; ++iter) {
        double alpha_iter = params->alpha * (1.0 - (double)iter / params->max_iters);
        if (alpha_iter < 1e-4) alpha_iter = 1e-4;

        // MOVE: escreve em pop_new[i], lê somente pop_old[j]
#ifdef _OPENMP
        if (use_openmp) {
#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; ++i) {
                // começa copiando o estado antigo
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
                solution_repair(pop_new[i], data);
            }
        } else
#endif
        {
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
                solution_repair(pop_new[i], data);
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

        // COST: calcula custo da nova pop
#ifdef _OPENMP
        if (use_openmp) {
#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; ++i) {
                cost_new[i] = ded_cost(pop_new[i], data);
            }
        } else
#endif
        {
            for (int i = 0; i < n; ++i) {
                cost_new[i] = ded_cost(pop_new[i], data);
            }
        }

        memcpy(pop_new[0]->P, best->P, n_vars * sizeof(double));
        cost_new[0] = best_cost;

        // BEST
        for (int i = 0; i < n; ++i) {
            if (cost_new[i] < best_cost) {
                best_cost = cost_new[i];
                memcpy(best->P, pop_new[i]->P, n_vars * sizeof(double));
            }
        }

        if ((iter + 1) % 500 == 0) {
            printf("iter=%d best_cost=%.10f\n", iter + 1, best_cost);
        }

        // SWAP buffers
        Solution **tmpP = pop_old; pop_old = pop_new; pop_new = tmpP;
        double *tmpC = cost_old; cost_old = cost_new; cost_new = tmpC;
    }

    // cleanup
    solution_destroy(trial);
    for (int i = 0; i < n; ++i) {
        solution_destroy(popA[i]);
        solution_destroy(popB[i]);
    }
    free(popA); free(popB);
    free(costA); free(costB);

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
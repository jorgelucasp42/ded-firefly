#include "firefly.h"
#include "utils.h"

#include <math.h>
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
                         Rng *rng) {
    size_t n_vars = (size_t)data->n_units * (size_t)data->n_hours;
    double r2 = solution_distance2(xi, xj, n_vars);
    double beta = params->beta0 * exp(-params->gamma * r2);

    for (int i = 0; i < data->n_units; ++i) {
        const UnitData *u = &data->units[i];
        double span = u->Pmax - u->Pmin;
        if (span <= 0.0) span = 1.0;

        for (int h = 0; h < data->n_hours; ++h) {
            size_t idx = (size_t)i * (size_t)data->n_hours + (size_t)h;
            double eps = params->alpha * (rng_uniform01(rng) - 0.5) * span;
            xi->P[idx] += beta * (xj->P[idx] - xi->P[idx]) + eps;
        }
    }

    solution_repair(xi, data);
}

static double run_firefly_core(const DedData *data,
                               const FireflyParams *params,
                               Solution *best,
                               int use_openmp) {
    int n = params->n_fireflies;
    size_t n_vars = (size_t)data->n_units * (size_t)data->n_hours;

    Solution **pop = (Solution **)calloc((size_t)n, sizeof(Solution *));
    double *cost = (double *)calloc((size_t)n, sizeof(double));
    if (!pop || !cost) {
        free(pop);
        free(cost);
        return 1e30;
    }

    Rng base;
    rng_seed(&base, params->seed);

    double best_cost = 1e30;
    for (int i = 0; i < n; ++i) {
        pop[i] = solution_create(data);
        if (!pop[i]) {
            best_cost = 1e30;
            n = i;
            goto cleanup;
        }
        solution_init_random(pop[i], data, rng_u32(&base));
        cost[i] = ded_cost(pop[i], data);
        if (cost[i] < best_cost) {
            best_cost = cost[i];
            memcpy(best->P, pop[i]->P, n_vars * sizeof(double));
        }
    }

    for (int iter = 0; iter < params->max_iters; ++iter) {
        // mover xi em direção a mais brilhantes xj
        if (use_openmp) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; ++i) {
                Rng rng;
                rng_seed(&rng, params->seed + (unsigned)(iter * 1315423911u + (unsigned)i * 2654435761u));

                for (int j = 0; j < n; ++j) {
                    if (cost[j] < cost[i]) {
                        move_firefly(data, params, pop[i], pop[j], &rng);
                    }
                }
            }
#else
            for (int i = 0; i < n; ++i) {
                Rng rng;
                rng_seed(&rng, params->seed + (unsigned)(iter * 1315423911u + (unsigned)i * 2654435761u));

                for (int j = 0; j < n; ++j) {
                    if (cost[j] < cost[i]) {
                        move_firefly(data, params, pop[i], pop[j], &rng);
                    }
                }
            }
#endif
        } else {
            for (int i = 0; i < n; ++i) {
                Rng rng;
                rng_seed(&rng, params->seed + (unsigned)(iter * 1315423911u + (unsigned)i * 2654435761u));

                for (int j = 0; j < n; ++j) {
                    if (cost[j] < cost[i]) {
                        move_firefly(data, params, pop[i], pop[j], &rng);
                    }
                }
            }
        }

        // recalcula custos
        if (use_openmp) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; ++i) {
                cost[i] = ded_cost(pop[i], data);
            }
#else
            for (int i = 0; i < n; ++i) {
                cost[i] = ded_cost(pop[i], data);
            }
#endif
        } else {
            for (int i = 0; i < n; ++i) {
                cost[i] = ded_cost(pop[i], data);
            }
        }

        // atualiza melhor
        for (int i = 0; i < n; ++i) {
            if (cost[i] < best_cost) {
                best_cost = cost[i];
                memcpy(best->P, pop[i]->P, n_vars * sizeof(double));
            }
        }
    }

cleanup:
    for (int i = 0; i < n; ++i) {
        solution_destroy(pop[i]);
    }
    free(pop);
    free(cost);
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
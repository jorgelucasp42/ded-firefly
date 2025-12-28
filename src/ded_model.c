#include "ded_model.h"
#include "utils.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static size_t total_vars(const DedData *data) {
    return (size_t)data->n_units * (size_t)data->n_hours;
}

Solution *solution_create(const DedData *data) {
    Solution *s = (Solution *)malloc(sizeof(Solution));
    if (!s) return NULL;

    size_t n = total_vars(data);
    s->P = (double *)calloc(n, sizeof(double));
    if (!s->P) {
        free(s);
        return NULL;
    }

    return s;
}

void solution_destroy(Solution *s) {
    if (!s) return;
    free(s->P);
    free(s);
}

void solution_init_random(Solution *s, const DedData *data, unsigned seed) {
    if (!s || !s->P) return;

    Rng rng;
    rng_seed(&rng, seed);

    for (int h = 0; h < data->n_hours; ++h) {
        double rem_load = data->load[h];
        double weights_sum = 0.0;
        double weights[MAX_UNITS];

        for (int i = 0; i < data->n_units; ++i) {
            const UnitData *u = &data->units[i];
            double w = 0.1 + rng_uniform01(&rng); // evitar zero
            weights[i] = w;
            weights_sum += w;

            size_t idx = (size_t)i * (size_t)data->n_hours + (size_t)h;
            s->P[idx] = u->Pmin;
            rem_load -= u->Pmin;
        }

        // distribui carga restante proporcionalmente à folga
        if (rem_load < 0) rem_load = 0;

        for (int i = 0; i < data->n_units; ++i) {
            const UnitData *u = &data->units[i];
            size_t idx = (size_t)i * (size_t)data->n_hours + (size_t)h;
            double headroom = u->Pmax - u->Pmin;
            double share = (weights[i] / weights_sum) * rem_load;
            if (share > headroom) share = headroom;
            s->P[idx] += share;
        }
    }

    solution_repair(s, data);
}

static inline double clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void solution_repair(Solution *s, const DedData *data) {
    if (!s || !s->P) return;

    int n_hours = data->n_hours;

    for (int h = 0; h < n_hours; ++h) {
        // Primeiro, respeita limites e rampas
        for (int i = 0; i < data->n_units; ++i) {
            const UnitData *u = &data->units[i];
            size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;

            double prev = (h == 0) ? u->P0 : s->P[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
            double lower = fmax(u->Pmin, prev - u->DR);
            double upper = fmin(u->Pmax, prev + u->UR);

            s->P[idx] = clamp(s->P[idx], lower, upper);
        }

        // Ajusta balanço de carga distribuindo diferença
        for (int iter = 0; iter < 50; ++iter) {
            double sum = 0.0;
            for (int i = 0; i < data->n_units; ++i) {
                size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
                sum += s->P[idx];
            }

            double diff = data->load[h] - sum;
            if (fabs(diff) < 1e-6) break;

            double total_room = 0.0;
            for (int i = 0; i < data->n_units; ++i) {
                const UnitData *u = &data->units[i];
                size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
                double prev = (h == 0) ? u->P0 : s->P[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
                double lower = fmax(u->Pmin, prev - u->DR);
                double upper = fmin(u->Pmax, prev + u->UR);

                if (diff > 0) {
                    total_room += (upper - s->P[idx]);
                } else {
                    total_room += (s->P[idx] - lower);
                }
            }

            if (total_room <= 1e-9) break;

            for (int i = 0; i < data->n_units; ++i) {
                const UnitData *u = &data->units[i];
                size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
                double prev = (h == 0) ? u->P0 : s->P[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
                double lower = fmax(u->Pmin, prev - u->DR);
                double upper = fmin(u->Pmax, prev + u->UR);

                double room = (diff > 0) ? (upper - s->P[idx]) : (s->P[idx] - lower);
                if (room <= 0) continue;

                double delta = (room / total_room) * diff;
                s->P[idx] = clamp(s->P[idx] + delta, lower, upper);
            }
        }
    }
}

double ded_cost(const Solution *s, const DedData *data) {
    if (!s || !s->P) return 1e30;

    double total = 0.0;
    int n_hours = data->n_hours;

    for (int h = 0; h < n_hours; ++h) {
        double sum_p = 0.0;
        for (int i = 0; i < data->n_units; ++i) {
            const UnitData *u = &data->units[i];
            size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
            double p = s->P[idx];

            double fuel = u->a + u->b * p + u->c * p * p + fabs(u->e * sin(u->f * (u->Pmin - p)));
            total += fuel;
            sum_p += p;
        }

        double diff = sum_p - data->load[h];
        if (fabs(diff) > 1e-3) {
            total += 1e6 * diff * diff; // penalidade pesada
        }
    }

    return total;
}
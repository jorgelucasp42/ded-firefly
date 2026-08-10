#include "ded_validation.h"

#include <math.h>
#include <stdio.h>

static void update_max(double value, double *max_value, int *max_unit, int *max_hour,
                       int unit, int hour) {
    if (value > *max_value) {
        *max_value = value;
        *max_unit = unit;
        *max_hour = hour;
    }
}

DedValidationReport ded_validate_solution(const DedData *data, const Solution *s) {
    DedValidationReport r = {0};

    r.max_balance_hour = -1;
    r.max_pmin_unit = r.max_pmin_hour = -1;
    r.max_pmax_unit = r.max_pmax_hour = -1;
    r.max_ramp_up_unit = r.max_ramp_up_hour = -1;
    r.max_ramp_down_unit = r.max_ramp_down_hour = -1;

    if (!data || !s || !s->P) {
        r.nan_count = 1;
        return r;
    }

    int n_hours = data->n_hours;

    for (int h = 0; h < n_hours; ++h) {
        double sum = 0.0;

        for (int i = 0; i < data->n_units; ++i) {
            size_t idx = (size_t)i * (size_t)n_hours + (size_t)h;
            double p = s->P[idx];

            if (isnan(p)) {
                r.nan_count++;
                continue;
            }
            if (isinf(p)) {
                r.inf_count++;
                continue;
            }

            sum += p;

            const UnitData *u = &data->units[i];
            double pmin_v = u->Pmin - p;
            double pmax_v = p - u->Pmax;
            if (pmin_v > 0.0) {
                update_max(pmin_v, &r.max_pmin_violation,
                           &r.max_pmin_unit, &r.max_pmin_hour, i, h);
            }
            if (pmax_v > 0.0) {
                update_max(pmax_v, &r.max_pmax_violation,
                           &r.max_pmax_unit, &r.max_pmax_hour, i, h);
            }

            double prev = (h == 0)
                        ? u->P0
                        : s->P[(size_t)i * (size_t)n_hours + (size_t)(h - 1)];
            if (isfinite(prev)) {
                double ramp_up_v = p - (prev + u->UR);
                double ramp_down_v = (prev - u->DR) - p;
                if (ramp_up_v > 0.0) {
                    update_max(ramp_up_v, &r.max_ramp_up_violation,
                               &r.max_ramp_up_unit, &r.max_ramp_up_hour, i, h);
                }
                if (ramp_down_v > 0.0) {
                    update_max(ramp_down_v, &r.max_ramp_down_violation,
                               &r.max_ramp_down_unit, &r.max_ramp_down_hour, i, h);
                }
            }
        }

        double balance_abs = fabs(sum - data->load[h]);
        if (balance_abs > r.max_balance_abs) {
            r.max_balance_abs = balance_abs;
            r.max_balance_hour = h;
            r.balance_sum_at_max = sum;
        }
    }

    return r;
}

void ded_print_validation_report(const DedData *data, const DedValidationReport *r) {
    if (!data || !r) return;

    printf("\n=== Validacao funcional da solucao final ===\n");
    printf("Max |sumP-load|: %.12f MW", r->max_balance_abs);
    if (r->max_balance_hour >= 0) {
        int h = r->max_balance_hour;
        printf(" (h=%d, load=%.6f, sumP=%.6f)",
               h, data->load[h], r->balance_sum_at_max);
    }
    printf("\n");

    printf("Max violacao Pmin: %.12f MW", r->max_pmin_violation);
    if (r->max_pmin_unit >= 0)
        printf(" (unit=%d, h=%d)", data->units[r->max_pmin_unit].id, r->max_pmin_hour);
    printf("\n");

    printf("Max violacao Pmax: %.12f MW", r->max_pmax_violation);
    if (r->max_pmax_unit >= 0)
        printf(" (unit=%d, h=%d)", data->units[r->max_pmax_unit].id, r->max_pmax_hour);
    printf("\n");

    printf("Max violacao rampa subida (UR): %.12f MW", r->max_ramp_up_violation);
    if (r->max_ramp_up_unit >= 0)
        printf(" (unit=%d, h=%d)", data->units[r->max_ramp_up_unit].id, r->max_ramp_up_hour);
    printf("\n");

    printf("Max violacao rampa descida (DR): %.12f MW", r->max_ramp_down_violation);
    if (r->max_ramp_down_unit >= 0)
        printf(" (unit=%d, h=%d)", data->units[r->max_ramp_down_unit].id, r->max_ramp_down_hour);
    printf("\n");

    printf("NaN: %d | Inf: %d\n", r->nan_count, r->inf_count);
    printf("Status validacao: %s\n",
           ded_validation_is_ok(r, 1e-5) ? "OK" : "VERIFICAR");
    printf("============================================\n");
}

int ded_validation_is_ok(const DedValidationReport *r, double tol) {
    if (!r) return 0;
    return r->nan_count == 0 &&
           r->inf_count == 0 &&
           r->max_balance_abs <= tol &&
           r->max_pmin_violation <= tol &&
           r->max_pmax_violation <= tol &&
           r->max_ramp_up_violation <= tol &&
           r->max_ramp_down_violation <= tol;
}

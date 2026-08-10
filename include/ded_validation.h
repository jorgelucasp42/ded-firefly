#ifndef DED_VALIDATION_H
#define DED_VALIDATION_H

#include "ded_data.h"
#include "ded_model.h"

typedef struct {
    double max_balance_abs;
    int max_balance_hour;
    double balance_sum_at_max;

    double max_pmin_violation;
    double max_pmax_violation;
    int max_pmin_unit;
    int max_pmin_hour;
    int max_pmax_unit;
    int max_pmax_hour;

    double max_ramp_up_violation;
    double max_ramp_down_violation;
    int max_ramp_up_unit;
    int max_ramp_up_hour;
    int max_ramp_down_unit;
    int max_ramp_down_hour;

    int nan_count;
    int inf_count;
} DedValidationReport;

DedValidationReport ded_validate_solution(const DedData *data, const Solution *s);
void ded_print_validation_report(const DedData *data, const DedValidationReport *r);
int ded_validation_is_ok(const DedValidationReport *r, double tol);

#endif

#ifndef DED_MODEL_H
#define DED_MODEL_H

#include "ded_data.h"

typedef struct {
    // P[i][h] em layout 1D: P[i * n_hours + h]
    double *P;
} Solution;

Solution *solution_create(const DedData *data);
void solution_destroy(Solution *s);

void solution_init_random(Solution *s, const DedData *data, unsigned seed);

// repara limites, rampas e balanço de carga (sem perdas)
void solution_repair(Solution *s, const DedData *data);

// custo total com efeito de válvula (sem perdas)
double ded_cost(const Solution *s, const DedData *data);

#endif

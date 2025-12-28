#ifndef FIREFLY_H
#define FIREFLY_H

#include "ded_data.h"
#include "ded_model.h"

typedef struct {
    int n_fireflies;
    int max_iters;
    double alpha;
    double beta0;
    double gamma;
    unsigned seed;
} FireflyParams;

double firefly_optimize(const DedData *data,
                        const FireflyParams *params,
                        Solution *best);

#endif

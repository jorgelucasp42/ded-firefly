#include <stdio.h>
#include "ded_data.h"
#include "ded_model.h"
#include "firefly.h"

int main(void) {
    DedData data = {0};

    if (ded_load_units(&data, "data/units_victoire100.csv") != 0) {
        fprintf(stderr, "Erro ao carregar units_victoire100.csv\n");
        return 1;
    }
    if (ded_load_load(&data, "data/load_24h_victoire100.csv") != 0) {
        fprintf(stderr, "Erro ao carregar load_24h_victoire100.csv\n");
        return 1;
    }

    Solution *best = solution_create(&data);

    FireflyParams params = {
        .n_fireflies = 40,
        .max_iters   = 500,
        .alpha       = 0.25,
        .beta0       = 1.0,
        .gamma       = 1e-3,
        .seed        = 1234u
    };

    double best_cost = firefly_optimize(&data, &params, best);

    printf("Melhor custo encontrado (seq): %.6f\n", best_cost);

    solution_destroy(best);
    return 0;
}

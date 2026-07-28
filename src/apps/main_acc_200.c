#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ded_data.h"
#include "ded_model.h"
#include "firefly.h"
#include "utils.h"

int main(int argc, char **argv) {
    DedData data = {0};

    const char *units_file = "data/units_victoire200.csv";
    const char *load_file  = "data/load_24h_victoire200.csv";

    unsigned seed = (argc > 1) ? (unsigned) atoi(argv[1]) : (unsigned) time(NULL);

    if (ded_load_units(&data, units_file) != 0) {
        fprintf(stderr, "Erro ao carregar %s\n", units_file);
        return 1;
    }

    if (ded_load_load(&data, load_file) != 0) {
        fprintf(stderr, "Erro ao carregar %s\n", load_file);
        return 1;
    }

    Solution *best = solution_create(&data);
    if (!best) {
        fprintf(stderr, "Erro ao criar solução.\n");
        return 1;
    }

    FireflyParams params = {
        .n_fireflies = 60,
        .max_iters   = 5000,
        .alpha       = 0.25,
        .beta0       = 1.0,
        .gamma       = 3e-7,
        .seed        = seed
    };

    double t0 = now_sec();

    double best_cost = firefly_optimize(&data, &params, best);

    double elapsed = now_sec() - t0;

    printf("\n=== Firefly OpenACC - DED (Victoire200) ===\n");
    printf("Dataset: %s / %s\n", units_file, load_file);
    printf("Seed: %u\n", seed);
    printf("Vagalumes: %d\n", params.n_fireflies);
    printf("Iterações: %d\n", params.max_iters);
    printf("Alpha: %.6f\n", params.alpha);
    printf("Beta0: %.6f\n", params.beta0);
    printf("Gamma: %.12g\n", params.gamma);
    printf("--------------------------------\n");
    printf("Melhor custo encontrado: %.6f\n", best_cost);
    printf("Tempo de execução: %.6f s\n", elapsed);
    printf("================================\n\n");

    solution_destroy(best);
    return 0;
}

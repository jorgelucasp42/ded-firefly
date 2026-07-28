#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ded_data.h"
#include "ded_model.h"
#include "firefly.h"
#include "utils.h"

int main(int argc, char **argv) {
    DedData data = {0};

    /* argv: seed  OU  units_file load_file seed */
    const char *units_file = (argc == 4) ? argv[1] : "data/units_victoire10.csv";
    const char *load_file  = (argc == 4) ? argv[2] : "data/load_24h_victoire10.csv";
    unsigned seed = (argc == 4) ? (unsigned)atoi(argv[3]) :
                    (argc == 2) ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL);

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
        .n_fireflies = 512,
        .max_iters   = 5000,
        .alpha       = 0.25,
        .beta0       = 1.0,
        .gamma       = 3e-7,
        .seed        = seed
    };

    double t0 = now_sec();

    double best_cost = firefly_optimize(&data, &params, best);

    double elapsed = now_sec() - t0;

    printf("\n=== Firefly OpenACC Pop512 - DED ===\n");
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ded_data.h"
#include "ded_model.h"
#include "firefly.h"
#include "utils.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Uso:\n");
    fprintf(stderr, "  %s [seed]\n", prog);
    fprintf(stderr, "  %s [seed] --alpha A --gamma G\n", prog);
    fprintf(stderr, "  %s units_file load_file seed [--alpha A --gamma G]\n", prog);
    fprintf(stderr, "Exemplo:\n");
    fprintf(stderr, "  %s 42 --alpha 0.10 --gamma 1e-6\n", prog);
}

int main(int argc, char **argv) {
    DedData data = {0};

    const char *units_file = "data/units_victoire10.csv";
    const char *load_file  = "data/load_24h_victoire10.csv";
    unsigned seed          = (unsigned)time(NULL);
    double alpha           = 0.25;
    double gamma_val       = 3e-7;

    /* primeira passagem: flags --alpha --gamma --help */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--alpha") == 0 && i + 1 < argc) {
            alpha = atof(argv[++i]);
        } else if (strcmp(argv[i], "--gamma") == 0 && i + 1 < argc) {
            gamma_val = atof(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* segunda passagem: argumentos posicionais */
    int pos = 0;
    for (int i = 1; i < argc; ++i) {
        /* pula flags e seus valores */
        if (strcmp(argv[i], "--alpha") == 0 ||
            strcmp(argv[i], "--gamma") == 0) { ++i; continue; }
        if (argv[i][0] == '-') continue;

        if (pos == 0) {
            /* seed (só dígitos) ou units_file */
            int is_num = 1;
            for (int k = 0; argv[i][k]; ++k)
                if (argv[i][k] < '0' || argv[i][k] > '9') { is_num = 0; break; }
            if (is_num) { seed = (unsigned)atoi(argv[i]); pos = 3; }
            else        { units_file = argv[i];            pos = 1; }
        } else if (pos == 1) {
            load_file = argv[i]; pos = 2;
        } else if (pos == 2) {
            seed = (unsigned)atoi(argv[i]); pos = 3;
        }
    }

    if (ded_load_units(&data, units_file) != 0) {
        fprintf(stderr, "Erro ao carregar %s\n", units_file);
        return 1;
    }
    if (ded_load_load(&data, load_file) != 0) {
        fprintf(stderr, "Erro ao carregar %s\n", load_file);
        return 1;
    }

    Solution *best = solution_create(&data);
    if (!best) { fprintf(stderr, "Erro ao criar solução.\n"); return 1; }

    FireflyParams params = {
        .n_fireflies = 60,
        .max_iters   = 5000,
        .alpha       = alpha,
        .beta0       = 1.0,
        .gamma       = gamma_val,
        .seed        = seed
    };

    double t0 = now_sec();
    double best_cost = firefly_optimize(&data, &params, best);
    double elapsed = now_sec() - t0;

    printf("\n=== Firefly Sequencial - DED ===\n");
    printf("Dataset: %s / %s\n", units_file, load_file);
    printf("Seed: %u\n", seed);
    printf("Vagalumes: %d\n", params.n_fireflies);
    printf("Iteracoes: %d\n", params.max_iters);
    printf("Alpha: %.6f\n", params.alpha);
    printf("Beta0: %.6f\n", params.beta0);
    printf("Gamma: %.12g\n", params.gamma);
    printf("--------------------------------\n");
    printf("Melhor custo encontrado: %.6f\n", best_cost);
    printf("Tempo de execucao: %.6f s\n", elapsed);
    printf("================================\n\n");

    solution_destroy(best);
    return 0;
}

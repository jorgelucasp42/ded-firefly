// Versão paralela usando OpenMP para o núcleo do Firefly
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include "ded_data.h"
#include "ded_model.h"
#include "firefly.h"
#include "utils.h"

int main(int argc, char **argv) {
    DedData data = {0};
    int n_fireflies = (argc > 1) ? atoi(argv[1]) : 60;
    unsigned seed = (argc > 2) ? (unsigned)atoi(argv[2]) : (unsigned)time(NULL);

    if (n_fireflies <= 0) {
        fprintf(stderr, "Numero de vagalumes deve ser maior que zero.\n");
        return 1;
    }

    if (ded_load_units(&data, "data/units_victoire100.csv") != 0) {
        fprintf(stderr, "Erro ao carregar units_victoire100.csv\n");
        return 1;
    }
    if (ded_load_load(&data, "data/load_24h_victoire100.csv") != 0) {
        fprintf(stderr, "Erro ao carregar load_24h_victoire100.csv\n");
        return 1;
    }

    Solution *best = solution_create(&data);
    if (!best) {
        fprintf(stderr, "Erro ao alocar solução\n");
        return 1;
    }

    FireflyParams params = {
        .n_fireflies = n_fireflies,
        .max_iters   = 5000,
        .alpha       = 0.25,
        .beta0       = 1.0,
        .gamma       = 3e-7,
        .seed        = seed
    };

    printf("Dataset: data/units_victoire100.csv / data/load_24h_victoire100.csv\n");
    printf("Vagalumes: %d\n", params.n_fireflies);
    printf("Seed: %u\n", params.seed);
    fflush(stdout);

    double t0 = now_sec();
    double best_cost = firefly_optimize_omp(&data, &params, best);
    double t1 = now_sec();

    int n_threads = 1;
#ifdef _OPENMP
    n_threads = omp_get_max_threads();
#endif

    printf("Melhor custo encontrado (omp, %d threads): %.6f\n", n_threads, best_cost);
    printf("Tempo: %.3f s\n", t1 - t0);

    solution_destroy(best);
    return 0;
}

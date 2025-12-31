// Versão paralela usando OpenMP para o núcleo do Firefly
#include <stdio.h>
#include <omp.h>
#include "ded_data.h"
#include "ded_model.h"
#include "firefly.h"
#include "utils.h"

int main(void) {
    DedData data = {0};

    if (ded_load_units(&data, "data/units_victoire30.csv") != 0) {
        fprintf(stderr, "Erro ao carregar units_victoire30.csv\n");
        return 1;
    }
    if (ded_load_load(&data, "data/load_24h_victoire30.csv") != 0) {
        fprintf(stderr, "Erro ao carregar load_24h_victoire30.csv\n");
        return 1;
    }

    Solution *best = solution_create(&data);
    if (!best) {
        fprintf(stderr, "Erro ao alocar solução\n");
        return 1;
    }

    FireflyParams params = {
        .n_fireflies = 40,
        .max_iters   = 500,
        .alpha       = 0.25,
        .beta0       = 1.0,
        .gamma       = 1e-3,
        .seed        = 1234u
    };

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
/* main_omp_bench.c — versao OMP configuravel para benchmark
 * Uso: ./bin/bin_omp_bench <n_fireflies> <seed> <max_iters> [tamanho_sistema]
 * tamanho_sistema: 10, 100 (default), 200 ou 500 unidades geradoras.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#include "ded_data.h"
#include "ded_model.h"
#include "ded_validation.h"
#include "firefly.h"
#include "utils.h"

int main(int argc, char **argv) {
    DedData data = {0};

    int      n_fireflies = (argc > 1) ? atoi(argv[1])           : 60;
    unsigned seed        = (argc > 2) ? (unsigned)atoi(argv[2]) : (unsigned)time(NULL);
    int      max_iters   = (argc > 3) ? atoi(argv[3])           : 5000;
    int      system_size = (argc > 4) ? atoi(argv[4])           : 100;

    if (n_fireflies <= 0 || max_iters <= 0) {
        fprintf(stderr, "Uso: %s <n_fireflies> <seed> <max_iters> [tamanho_sistema]\n", argv[0]);
        return 1;
    }

    char units_file[64], load_file[64];
    switch (system_size) {
        case 10: case 100: case 200: case 500:
            snprintf(units_file, sizeof(units_file), "data/units_victoire%d.csv", system_size);
            snprintf(load_file,  sizeof(load_file),  "data/load_24h_victoire%d.csv", system_size);
            break;
        default:
            fprintf(stderr, "Tamanho de sistema invalido: %d (use 10, 100, 200 ou 500)\n", system_size);
            return 1;
    }

    if (ded_load_units(&data, units_file) != 0) {
        fprintf(stderr, "Erro ao carregar %s\n", units_file); return 1;
    }
    if (ded_load_load(&data, load_file) != 0) {
        fprintf(stderr, "Erro ao carregar %s\n", load_file); return 1;
    }

    Solution *best = solution_create(&data);
    if (!best) { fprintf(stderr, "Erro ao criar solucao.\n"); return 1; }

    FireflyParams params = {
        .n_fireflies = n_fireflies,
        .max_iters   = max_iters,
        .alpha       = 0.25,
        .beta0       = 1.0,
        .gamma       = 3e-7,
        .seed        = seed
    };

    int n_threads = omp_get_max_threads();
    printf("BENCH_OMP f=%d seed=%u iters=%d threads=%d sistema=%d\n",
           n_fireflies, seed, max_iters, n_threads, system_size);
    fflush(stdout);

    double t0 = now_sec();
    double best_cost = firefly_optimize_omp(&data, &params, best);
    double elapsed = now_sec() - t0;

    printf("RESULT_COST=%.10f\n", best_cost);
    printf("RESULT_TIME=%.6f\n",  elapsed);
    printf("RESULT_ALPHA=%.6f\n", params.alpha);
    printf("RESULT_BETA0=%.6f\n", params.beta0);
    printf("RESULT_GAMMA=%.12g\n", params.gamma);
    printf("RESULT_N_UNITS=%d\n", data.n_units);
    printf("RESULT_N_HOURS=%d\n", data.n_hours);
    DedValidationReport validation = ded_validate_solution(&data, best);
    printf("RESULT_VALID_OK=%d\n", ded_validation_is_ok(&validation, 1e-5));
    printf("RESULT_MAX_BALANCE=%.12f\n", validation.max_balance_abs);
    printf("RESULT_MAX_PMIN=%.12f\n", validation.max_pmin_violation);
    printf("RESULT_MAX_PMAX=%.12f\n", validation.max_pmax_violation);
    printf("RESULT_MAX_RAMP_UP=%.12f\n", validation.max_ramp_up_violation);
    printf("RESULT_MAX_RAMP_DOWN=%.12f\n", validation.max_ramp_down_violation);
    printf("RESULT_NAN=%d\n", validation.nan_count);
    printf("RESULT_INF=%d\n", validation.inf_count);

    solution_destroy(best);
    return 0;
}

// Programa simples para validar leitura de dados e função de custo
#include <stdio.h>
#include "ded_data.h"
#include "ded_model.h"

int main(void) {
    DedData data = {0};

    if (ded_load_units(&data, "data/units_victoire10.csv") != 0) {
        fprintf(stderr, "Falha ao ler units_victoire10.csv\n");
        return 1;
    }
    if (ded_load_load(&data, "data/load_24h_victoire10.csv") != 0) {
        fprintf(stderr, "Falha ao ler load_24h_victoire10.csv\n");
        return 1;
    }

    Solution *s = solution_create(&data);
    if (!s) {
        fprintf(stderr, "Erro de alocação\n");
        return 1;
    }

    solution_init_random(s, &data, 2025u);
    double cost = ded_cost(s, &data);

    printf("Unidades: %d, Horas: %d\n", data.n_units, data.n_hours);
    printf("Custo de uma solução inicial reparada: %.6f\n", cost);

    // imprime geração na hora 1
    printf("Geração na hora 1:\n");
    for (int i = 0; i < data.n_units; ++i) {
        double p = s->P[(size_t)i * (size_t)data.n_hours + 0];
        printf("  Unidade %d: %.3f MW\n", data.units[i].id, p);
    }

    solution_destroy(s);
    return 0;
}
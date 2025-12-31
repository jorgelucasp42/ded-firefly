// Programa simples para validar leitura de dados, repair e função de custo
#include <stdio.h>
#include <math.h>
#include <stddef.h>

#include "ded_data.h"
#include "ded_model.h"

int main(void)
{
    DedData data = (DedData){0};

    if (ded_load_units(&data, "data/units_victoire50.csv") != 0)
    {
        fprintf(stderr, "Falha ao ler units_victoire50.csv\n");
        return 1;
    }
    if (ded_load_load(&data, "data/load_24h_victoire50.csv") != 0)
    {
        fprintf(stderr, "Falha ao ler load_24h_victoire50.csv\n");
        return 1;
    }

    Solution *s = solution_create(&data);
    if (!s)
    {
        fprintf(stderr, "Erro de alocação\n");
        return 1;
    }

    solution_init_random(s, &data, 2025u);
    double cost = ded_cost(s, &data);

    printf("Unidades: %d, Horas: %d\n", data.n_units, data.n_hours);
    printf("Custo de uma solução inicial reparada: %.6f\n", cost);

    // imprime geração na hora 1 (h=0)
    printf("\nGeração na hora 1 (h=0):\n");
    for (int i = 0; i < data.n_units; ++i)
    {
        size_t idx = (size_t)i * (size_t)data.n_hours + 0u;
        printf("  Unidade %d: %.3f MW\n", data.units[i].id, s->P[idx]);
    }

    // valida balanço em todas as horas
    double max_err = 0.0;
    int max_h = 0;
    double sum_at_max = 0.0;

    printf("\nChecagem do balanço de carga por hora:\n");
    printf("  h   load        sumP        |diff|\n");
    for (int h = 0; h < data.n_hours; ++h)
    {
        double sum = 0.0;
        for (int i = 0; i < data.n_units; ++i)
        {
            size_t idx = (size_t)i * (size_t)data.n_hours + (size_t)h;
            sum += s->P[idx];
        }
        double diff = sum - data.load[h];
        double adiff = fabs(diff);

        // imprime algumas linhas (todas, pra debug)
        printf(" %2d  %9.3f  %9.3f  %9.6f\n", h, data.load[h], sum, adiff);

        if (adiff > max_err)
        {
            max_err = adiff;
            max_h = h;
            sum_at_max = sum;
        }
    }

    printf("\nMax |sumP - load| nas 24h: %.12f (h=%d, load=%.6f, sumP=%.6f)\n",
           max_err, max_h, data.load[max_h], sum_at_max);

    double max_ramp_violation = 0.0;

    for (int i = 0; i < data.n_units; ++i)
    {
        const UnitData *u = &data.units[i];
        for (int h = 0; h < data.n_hours; ++h)
        {
            double prev = (h == 0) ? u->P0
                                   : s->P[(size_t)i * (size_t)data.n_hours + (size_t)(h - 1)];
            double curr = s->P[(size_t)i * (size_t)data.n_hours + (size_t)h];

            // violações: curr > prev+UR ou curr < prev-DR
            double v1 = curr - (prev + u->UR);
            double v2 = (prev - u->DR) - curr;

            if (v1 > max_ramp_violation)
                max_ramp_violation = v1;
            if (v2 > max_ramp_violation)
                max_ramp_violation = v2;
        }
    }

    printf("Max violação de rampa (UR/DR): %.12f\n", max_ramp_violation);

    solution_destroy(s);
    return 0;
}
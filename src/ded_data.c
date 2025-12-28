#include "ded_data.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int is_header_line(const char *line) {
    // Se começa com letra, provavelmente é header
    while (*line == ' ' || *line == '\t') line++;
    return (*line >= 'A' && *line <= 'z');
}

int ded_load_units(DedData *data, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 1;

    char buf[1024];
    int count = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#') continue;
        if (is_header_line(buf)) continue;

        // Espera: id,Pmin,Pmax,a,b,c,e,f,UR,DR,P0
        UnitData u = {0};
        int n = sscanf(buf, " %d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                       &u.id, &u.Pmin, &u.Pmax, &u.a, &u.b, &u.c,
                       &u.e, &u.f, &u.UR, &u.DR, &u.P0);

        if (n != 11) {
            // tenta também com ponto-e-vírgula
            n = sscanf(buf, " %d;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf",
                       &u.id, &u.Pmin, &u.Pmax, &u.a, &u.b, &u.c,
                       &u.e, &u.f, &u.UR, &u.DR, &u.P0);
        }
        if (n != 11) {
            fclose(fp);
            return 2;
        }

        if (count >= MAX_UNITS) {
            fclose(fp);
            return 3;
        }

        data->units[count++] = u;
    }

    fclose(fp);
    data->n_units = count;
    return (count > 0) ? 0 : 4;
}

int ded_load_load(DedData *data, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 1;

    char buf[512];
    int count = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#') continue;
        if (is_header_line(buf)) continue;

        int hour = 0;
        double pd = 0.0;

        // Espera: hour,PD
        int n = sscanf(buf, " %d,%lf", &hour, &pd);
        if (n != 2) {
            n = sscanf(buf, " %d;%lf", &hour, &pd);
        }
        if (n != 2) {
            fclose(fp);
            return 2;
        }

        if (count >= MAX_HOURS) {
            fclose(fp);
            return 3;
        }

        data->load[count++] = pd;
    }

    fclose(fp);
    data->n_hours = count;
    return (count > 0) ? 0 : 4;
}

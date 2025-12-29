#ifndef DED_DATA_H
#define DED_DATA_H

#include <stddef.h>

#define MAX_UNITS  50
#define MAX_HOURS  24

typedef struct {
    int    id;
    double Pmin, Pmax;
    double a, b, c;
    double e, f;     // valve-point
    double UR, DR;   // ramp up / ramp down
    double P0;       // potência inicial (antes da hora 1)
} UnitData;

typedef struct {
    int    n_units; // 10
    int    n_hours; // 24
    UnitData units[MAX_UNITS];
    double  load[MAX_HOURS]; // demanda por hora
} DedData;

int ded_load_units(DedData *data, const char *path);
int ded_load_load(DedData *data, const char *path);

#endif

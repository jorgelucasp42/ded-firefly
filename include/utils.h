#ifndef UTILS_H
#define UTILS_H

#include <time.h>

double now_sec(void);

// RNG simples (LCG) determinístico
typedef struct {
    unsigned s;
} Rng;

static inline void rng_seed(Rng *r, unsigned seed) { r->s = seed ? seed : 1u; }

static inline unsigned rng_u32(Rng *r) {
    r->s = 1664525u * r->s + 1013904223u;
    return r->s;
}

static inline double rng_uniform01(Rng *r) {
    // [0,1)
    unsigned x = (rng_u32(r) >> 8) & 0x00FFFFFFu;
    return (double)x / (double)0x01000000u;
}

#endif

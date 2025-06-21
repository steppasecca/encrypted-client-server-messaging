#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h>

typedef struct Blocks {
    unsigned int n;
    uint64_t *array;
} Blocks;

typedef struct ThreadArgs {
    Blocks *partition;
    uint64_t k;
} ThreadArgs;

#endif

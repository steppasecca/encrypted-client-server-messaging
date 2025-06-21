#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdint.h>
#include <signal.h>

typedef struct Blocks Blocks;

int load_file(char *input_path, Blocks *blocks);

int manage_encryption(int p, uint64_t k, Blocks *blocks);

int connection(char *target_address, Blocks *blocks, uint64_t k);

const sigset_t * init_sigset();

#endif

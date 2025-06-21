#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <signal.h>
#include <pthread.h>

typedef struct SockQueue SockQueue;
typedef struct BlockList BlockList;
typedef struct OutputBuffer OutputBuffer;
typedef struct WorkerArgs WorkerArgs;

const sigset_t * init_sigset();

int init_workers(WorkerArgs *worker_args, int l);

int manage_connections(SockQueue *queue, int l);

int receive_blocks(int sock_fd, BlockList *block_list);

int init_decryption(BlockList *block_list, int p);

void *worker_loop(void *args);

int setup_signal_handling();

int write_file(char *output_path, OutputBuffer *output_buffer);

#endif

#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h> 
#include <pthread.h>

typedef struct SockNode {
    int fd;
    struct SockNode *next;
} SockNode;

typedef struct SockQueue {
    SockNode *head;
    SockNode *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
} SockQueue;

typedef struct BlockNode {
    uint64_t block;
    struct BlockNode *next;
} BlockNode;

typedef struct BlockList {
    uint32_t len;
    uint64_t k;
    BlockNode *head;
    BlockNode *tail;
} BlockList;

typedef struct OutputBuffer {
    BlockNode *head;
    BlockNode *tail;
    pthread_mutex_t mutex;
} OutputBuffer;

// Thread args

typedef struct WorkerArgs {
    SockQueue *queue;
    OutputBuffer *output_buffer;
    int p;
    int l;
}   WorkerArgs;

typedef struct DecryptArgs {
    size_t block_per_thread;
    BlockNode *partition_head;
    uint64_t k;
} DecryptArgs;

#endif

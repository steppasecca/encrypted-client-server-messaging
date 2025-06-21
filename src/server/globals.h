#ifndef GLOBALS_H
#define GLOBALS_H

#include <signal.h>
#include <pthread.h>

extern volatile sig_atomic_t shutdown_server_flag;
extern volatile int idle_workers; 
extern pthread_mutex_t idle_mutex;
extern pthread_cond_t all_idle_cond;

#endif

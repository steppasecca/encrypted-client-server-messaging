#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include "functions.h"
#include "structs.h"
#include "globals.h"


void *worker_loop(void *arg_ptr){

    // Casting
    WorkerArgs *args = (WorkerArgs *)arg_ptr;
    OutputBuffer *output_buffer = args->output_buffer;
    SockQueue *queue = args->queue;    
    int p = args->p;
    int l = args->l;

    // Inizializzazione variabili per il ciclo
    SockNode *sock_node = NULL;
    int sock_fd = -1;    
    BlockList *block_list = NULL;
    BlockNode *div_node = NULL;
    
    // Ciclo per l'attesa di connessioni
    while (1){

	// Incremento idle_workers
	pthread_mutex_lock(&idle_mutex);
        idle_workers++;
        if (idle_workers == l) {
            pthread_cond_signal(&all_idle_cond);
        }
        pthread_mutex_unlock(&idle_mutex);

	// Lock e attesa se la coda è vuota
	pthread_mutex_lock(&queue->mutex);
	while (queue->head == NULL){
	    pthread_cond_wait(&queue->cond_not_empty, &queue->mutex);
	}

	// Decremento idle_workers
	pthread_mutex_lock(&idle_mutex);
        idle_workers--;
        pthread_mutex_unlock(&idle_mutex);

	// Estrazione del primo nodo della coda
	sock_node = queue->head;
	queue->head = sock_node->next;
	if (queue->head == NULL) queue->tail = NULL;

	// Unlock
	pthread_mutex_unlock(&queue->mutex);

	// Estrazione del valore e pulizia del nodo estratto
	sock_fd = sock_node->fd;
	free(sock_node);

	// Allocazione del buffer di ricezione dei blocchi crittati
	block_list = (BlockList *)malloc(sizeof(BlockList));
	if (block_list == NULL){
	    fprintf(stderr, "Errore di allocazione risorse");
	    goto cleanup;
	}
	block_list->len = 0;
	block_list->k = 0;
	block_list->head = NULL;
	block_list->tail = NULL;

	if (receive_blocks(sock_fd, block_list)){
	    fprintf(stderr,"Errore in receive_blocks\n");
	    goto cleanup;
	}
	
	// Decrittazione parallela 
	if (init_decryption(block_list, p)){
	    fprintf(stderr,"Errore in init_decryption\n");
	    goto cleanup;
	}

	
	// Creazione nodo divisore tra file
	div_node = (BlockNode *)malloc(sizeof(BlockNode));
	div_node->block = 0;
	div_node->next = NULL;
	
	// Aggiunta di block_list al buffer condiviso tramite concatenazione di liste
	pthread_mutex_lock(&output_buffer->mutex);
	output_buffer->tail->next = block_list->head;
	block_list->tail->next = div_node;
	output_buffer->tail = div_node;
	pthread_mutex_unlock(&output_buffer->mutex);	

	if (sock_fd != -1) {
	    close(sock_fd);
	    sock_fd = -1;
	}
	
	// Deallocazione di block_list ma non dei nodi
	if (block_list != NULL) free(block_list);	
    }

 cleanup:
    
    if (sock_fd != -1) close(sock_fd);
    
    if (block_list != NULL) {
	BlockNode *current_node = block_list->head;
	BlockNode *next_node;
	while(current_node!=NULL){
	    next_node = current_node->next;
	    free(current_node);
	    current_node = next_node;
	}
	free(block_list);
    }
    return NULL;
}

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


void *decrypt(void *args_ptr){

	// Casting args
	DecryptArgs *args = (DecryptArgs *)args_ptr;
	size_t b = args->block_per_thread;
	BlockNode *node = args->partition_head;
	uint64_t k = args->k;

	for (int i = 0; i < b; i++){
		node->block ^= k;
		node = node->next;
	}

	return NULL;
}


int init_decryption(BlockList *block_list, int p){

	int ret_val =1;

	// Calcolo parziale del numero di blocchi per ogni thread
	size_t partial = block_list->len / p;
	size_t remainder = block_list->len % p;

	// Allocazione risorse
	DecryptArgs *args_array = (DecryptArgs *)malloc(p * sizeof(DecryptArgs));
	if (args_array == NULL){
		fprintf(stderr, "Errore nell'allocazione della memoria: args_array\n");
		goto cleanup;
	}
	pthread_t *pids = (pthread_t *)malloc(p * sizeof(pthread_t));
	if (pids == NULL){
		fprintf(stderr, "Errore nell'allocazione della memoria: pids\n");
		goto cleanup;
	}

	// Costruzione iteratore per la selezione dei nodi
	BlockNode *iterator = block_list->head;

	// Ciclo per la creazione di threads
	int active_threads = 0;
	for (int i = 0; i < p; i++){

		// Calcolo esatto del numero di blocchi per ogni thread
		size_t blocks_per_thread = partial + (i < remainder ? 1 : 0);

		// Selezione del nodo head per il thread e scorrimento della lista
		// Scorro prima perchè altrimenti c'è race condition
		BlockNode *partition_head = iterator;
		for (int j = 0; j < blocks_per_thread; j++){
			iterator = iterator->next;
		}

		// Casting degli argomenti
		args_array[i].block_per_thread = blocks_per_thread;
		args_array[i].partition_head = partition_head;
		args_array[i].k = block_list->k;

		// Creazione del thread
		if (pthread_create(&pids[i], NULL, decrypt, &args_array[i]) != 0){
			perror("pthread_create");
			goto join_and_cleanup;
		}

		// Aggiornamento del numero di threads attivi
		active_threads++;
	}

	// Chiusura funzione
	// Successo

	ret_val = 0;

join_and_cleanup:

	// Ciclo di attesa per la terminazione di tutti i threads
	for (int i=0; i < active_threads; i++){
		pthread_join(pids[i], NULL);
	}

cleanup:

	// Deallocazione delle risorse
	if (args_array) free(args_array);
	if (pids) free(pids);

	return ret_val;
}

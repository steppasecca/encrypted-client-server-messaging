#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include "functions.h"
#include "structs.h"

int init_workers(WorkerArgs *worker_args, int l){

	int ret_val = 1;

	// Inizializzazione maschera di bloccaggio dei segnali
	int sigmask_applied = 0;
	sigset_t old_sig_set;
	const sigset_t *sig_set = init_sigset();

	// Setting maschera di bloccaggio dei segnali
	if (pthread_sigmask(SIG_BLOCK, sig_set, &old_sig_set) == -1) {
		perror("Errore in pthread_sigmask");
		goto cleanup;
	}
	sigmask_applied = 1;

	// Ciclo per la creazione dei workers
	for (int i = 0; i < l; i++){
		pthread_t pid;
		if (pthread_create(&pid, NULL, worker_loop, (void *)worker_args) != 0){
			perror("pthread_create");
			goto cleanup;
		}
	}

	ret_val = 0;

cleanup:

	if (sigmask_applied) pthread_sigmask(SIG_SETMASK, &old_sig_set, NULL);

	return ret_val;
}

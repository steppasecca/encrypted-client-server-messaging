#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h> 

#include "structs.h"
#include "functions.h"

// Puntatore a funzione di tipo void* utilizzata dai threads per crittare
void *encrypt(void *void_arg){

    // Casting degli argomenti
    ThreadArgs *args = (ThreadArgs *)void_arg;
    Blocks *partition = args->partition;
    uint64_t k = args->k;

    // Ciclo di cifratura che opera in loco (all'interno di ogni blocco della variabile blocks)
    for(int i=0; i<partition->n; i++){
        partition->array[i] ^= k;
    }

    return NULL;
}


int manage_encryption(int p, uint64_t k, Blocks *blocks){

    // Dichiarazione e inizializzazione del valore di ritorno
    int ret_val = 1;
    
    // Calcolo approssimativo 
    int partition_blocks= blocks->n /p;
    int remainder= blocks->n % p;

    // Allocazione dinamica delle risorse
    ThreadArgs *args_array = (ThreadArgs *)malloc(p * sizeof(ThreadArgs));
    Blocks *partition=(Blocks *)malloc(p*sizeof(Blocks));
    pthread_t *tids = (pthread_t *)malloc(p * sizeof(pthread_t));

    if (args_array == NULL || partition == NULL || tids == NULL) {
	fprintf(stderr, "Errore nell'allocazione dinamica della memoria");
	goto cleanup;
    }

    // Dichiarazione del set di segnali da bloccare e 
    // del set di segnali da ripristinare al termine dell'operazione
    sigset_t old_sig_set;

    // Ottengo il puntatore alla maschera di bloccaggio (o la inizializzo)
    const sigset_t *sig_set_ptr = init_sigset();

    // Setting effettivo della maschera di bloccaggio
    if (pthread_sigmask(SIG_BLOCK, sig_set_ptr, &old_sig_set) == -1){
	perror("Errore nel setting della maschera di bloccaggio.");
	goto cleanup;
    }


    // Ciclo per la creazione di threads paralleli
    int active_threads = 0;
    int array_index = 0;
    for (int i=0; i<p; i++){

	// Calcolo della quantità di blocchi da dare in input (suddivide il resto equamente nei primi i partitions)
	int l = partition_blocks + (i < remainder ? 1 : 0);

	// Partizioni
	partition[i].n = l;
	partition[i].array = &(blocks->array[array_index]);

	// Setting degli argomenti 
	args_array[i].partition = &partition[i];
	args_array[i].k = k;
	    
	// Creazione del thread
	if (pthread_create(&tids[i], NULL, encrypt, &args_array[i]) != 0){
	    perror("Creazione thread non riuscita.");
	    goto join_and_cleanup;
    	}

	// Aggiornamento del numero di threads secondari attivi
	active_threads++;

	// Aggiornamento dell'indice dell'array sulla base del numero di blocchi dati in input al thread
	array_index = array_index+l;
    }

    
    // Chiusura funzione
    // Successo
    ret_val = 0;

    
 join_and_cleanup:
    
    // Ciclo di attesa per la terminazione di tutti i threads
    for (int i=0; i < active_threads; i++){
	pthread_join(tids[i], NULL);
    }

    // Ripristino della maschera dei segnali
    pthread_sigmask(SIG_SETMASK, &old_sig_set, NULL);

 cleanup:

    // Deallocazione delle risorse
    free(args_array);
    free(tids);
    free(partition);
    
    return ret_val;
}

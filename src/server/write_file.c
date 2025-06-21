#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h> 
#include <getopt.h>
#include <pthread.h>

#include "structs.h"
#include "functions.h"


int write_file(char *output_path, OutputBuffer *output_buffer){

    int ret_val = 1;
	
    // Check validità argomenti
    
    if (output_buffer == NULL){
	fprintf(stderr, "output_buffer == NULL\n");
	goto close;
    }

    if (output_path == NULL){
	fprintf(stderr, "output_path == NULL\n");
	goto close;
    }
    
    
    // Creazione file di output
    int open = 0;
    FILE *fp = fopen(output_path, "w");
    if (fp == NULL) {
	perror("fopen");
	goto close;
    }
    open = 1;
	
    // Blocco l'accesso al buffer per evitare condizioni di gara
    int locked = 0;
    if (pthread_mutex_lock(&output_buffer->mutex) != 0) {
	perror("pthread_mutex_lock");
	goto close;
    }
    locked = 1;

    // Inizializzazione dell'iteratore
    BlockNode *b_iterator = output_buffer->head;
    char write_buffer[sizeof(uint64_t)];
    
    // Ciclo di scrittura
    while (b_iterator != NULL) {
	     
	if (b_iterator->block == 0) {	    
	    if (fputc('\n', fp) == EOF) {
		perror("fputc");
		goto close; 
	    }
	    
	} else {
            memcpy(write_buffer, &(b_iterator->block), sizeof(uint64_t));	    
	    if (fwrite(write_buffer, sizeof(char), 8, fp) != 8) {
		perror("fwrite");
		goto close;
	    }
	}
	
	// Scorrimento lista
	b_iterator = b_iterator->next;
    }

    // Chiusura funzione
    ret_val = 0;
    
 close:
    
    if (locked) pthread_mutex_unlock(&output_buffer->mutex);

    if (open) fclose(fp);

    return ret_val;
}

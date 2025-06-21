#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "structs.h"

int load_file(char *input_path,Blocks *blocks){
    
    FILE *input_file= fopen(input_path, "rb");
    if (input_file == NULL){
        fprintf(stderr, "Errore: file '%s' non trovato o non apribile.\n", input_path);
	return 1;
    }

    // Calcolo della dimensione del file
    fseek(input_file,0L,SEEK_END);    
    long file_size = ftell(input_file);
    if (file_size == -1L){
	perror("Errore nel calcolo delle dimensioni del file.");
	return 1;
    }
    rewind(input_file);

    // Calcolo del numero dei blocchi in cui suddividere il contenuto del file
    blocks->n = (file_size + sizeof(uint64_t) - 1) / sizeof(uint64_t);

    // Allocazione dell'array di puntatori in cui immagazzinare i blocchi di bytes
    blocks->array = (uint64_t *)calloc(blocks->n, sizeof(uint64_t));
    if (blocks->array == NULL) {
        fprintf(stderr, "Errore: impossibile allocare memoria per l'array di blocchi.\n");
        fclose(input_file);
        return 1;
    }

    // Lettura del file e scrittura nell'array di blocchi
    size_t bytes_read = fread(blocks->array, 1, file_size, input_file);
    if (bytes_read != file_size) {
        fprintf(stderr, "Errore durante la lettura del file. Letti %zu di %ld byte.\n", bytes_read, file_size);
        free(blocks->array); // Libera la memoria in caso di errore
        blocks->array = NULL;
        fclose(input_file);
        return 1;
    }

    fclose(input_file);

    return 0;
}


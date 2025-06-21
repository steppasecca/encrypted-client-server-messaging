#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "functions.h"
#include "structs.h"


int c_recv(int sock_fd, unsigned char *data_ptr, size_t data_size){
    
    ssize_t received = recv(sock_fd, data_ptr, data_size, MSG_WAITALL);
    if (received < 0) {
    	perror("recv");
    	return 1;
    }
    if ((size_t)received != data_size){
    	fprintf(stderr, "Ricevuti %zd bytes, attesi %zu\n", received, data_size);
	return 1;
    }

    return 0;
}


int receive_blocks(int sock_fd, BlockList *block_list) {

    int ret_val = 1;

    // Inizializzazione della lista per la scrittura dei blocchi da ricevere
    BlockNode **node_ptr = &(block_list->head);

    // Ricezione del numero di blocchi da ricevere
    uint32_t len = 0;
    if (c_recv(sock_fd, (unsigned char *)&len, sizeof(uint32_t)) != 0) {
    	fprintf(stderr, "Errore nella ricezione della dimensione\n");
	goto cleanup;
    }
    block_list->len = ntohl(len);
    
    // Ricezione dei blocchi
    for (int i = 0; i < block_list->len; i++) {

    	*node_ptr = (BlockNode *)malloc(sizeof(BlockNode));
	if (*node_ptr == NULL) {
            fprintf(stderr, "Errore di allocazione per BlockNode\n");	    
            goto cleanup;
        }

	(*node_ptr)->next = NULL;

	if (c_recv(sock_fd, (unsigned char *) &((*node_ptr)->block), sizeof(uint64_t))) {
	    fprintf(stderr, "Errore nella ricezione del blocco numero %d\n", i);
	    free(*node_ptr);
            *node_ptr = NULL; 
	    goto cleanup;
	}

	// Il nodo puntato da node_ptr è sempre l'ultimo (aggiorno la coda ad ogni iterazione)	
	block_list->tail = *node_ptr;

	// Scorro la lista sul prossimo nodo (non ancora allocato)
	node_ptr = &((*node_ptr)->next);
    }

    // Ricezione della chiave di cifratura
    if (c_recv(sock_fd, (unsigned char *)&block_list->k, sizeof(uint64_t))) {
	fprintf(stderr, "Errore nella ricezione della chiave di cifratura\n");
	goto cleanup;	
    }

    // Invio ACK
    ssize_t sent_bytes = send(sock_fd, "ACK", 3, 0);
    if (sent_bytes < 3) {
	perror("send");
	goto cleanup;
    }    

    // Chiusura funzione
    // Successo
    ret_val = 0;
    
 cleanup:

    if (ret_val != 0) { // Indica un percorso d'errore
        BlockNode *current = block_list->head;
        BlockNode *next;
        while(current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }
        block_list->head = NULL;
        block_list->tail = NULL;
    }
    
    return ret_val;
}

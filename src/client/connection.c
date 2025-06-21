// Valutiamo se conviene mandare l'intero array con c_send e non blocco per blocco
// Perchè così risparmiamo sulle chiamate di c_send -j

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "functions.h"
#include "structs.h"

#define LOCAL_HOST "127.0.0.1"
#define PORT 1312

int c_send(int client_fd, unsigned char *data_ptr, size_t data_size);
int send_blocks(int client_fd, Blocks *blocks, uint64_t k);
int receive_ack(int client_fd);

int connection(char *target_address, Blocks *blocks, uint64_t k){

    // Dichiarazione e inizializzazione del valore di ritorno
    int ret_val = 1;

    // Dichiarazione variabili per l'indirizzo target
    char host[22];
    memset(host,0,sizeof(host));
    int port;

    // Analisi e splittaggio dell'indirizzo in input 
    if(target_address == NULL){
	strcpy(host,LOCAL_HOST);
	port=PORT;
    } else {
	if (sscanf(target_address, "%21[^:]:%d", host, &port) != 2){ 
	    fprintf(stderr,"Formato dell'indirizzo target non valido.\n");
	    goto cleanup;
	}
	if (port<=1024 || port>65536){
	    fprintf(stderr,"Valore porta non valido. Richiesto valore compreso tra 1025 e 65535.\n");
	    goto cleanup;
	}
    }

    // Creazione socket

    // Inizializzazione del file descriptor
    int client_fd = -1;

    // Istanziazione della struttura sockaddr_in
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Analisi e conversione del formato dell'indirizzo IP di destinazione
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) { 
	perror("Indirizzo IP non valido o non supportato.");
	goto cleanup;
    }

    // Creazione effettiva del socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
	perror("Creazione del socket fallita.");
	goto cleanup;
    }

    // Connessione al server
    int connection_attemps = 0;
    int sleep_time = 1; // Inizia con 1 secondo
    while (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	perror("Tentativo di connessione fallito");
	if (++connection_attemps == 5) {
	    fprintf(stderr, "Errore: numero massimo di tentativi raggiunto.\n");
	    goto cleanup;
	}
	sleep(sleep_time);
	sleep_time *= 2; 
    }

    // Invio dei blocchi via socket
    if (send_blocks(client_fd, blocks, k)){
	goto cleanup;
    }

    // Attesa ricezione ACK
    if (receive_ack(client_fd)){
	goto cleanup;
    }

    // Termine della funzione
    // Valore successo
    ret_val = 0;

    // Blocco cleanup
 cleanup: 
    if (client_fd != -1) {
	close(client_fd);
    }

    return ret_val;
}


int send_blocks(int client_fd, Blocks *blocks, uint64_t k){

    // Dichiarazione e inizializzazione del valore di ritorno
    int ret_val = 1;

    // Gestione dei segnali di interruzione

    // Dichiarazione del set di segnali da bloccare e del set di segnali da ripristinare al termine dell'operazione
    sigset_t old_sig_set;

    // Ottengo il puntatore alla maschera di bloccaggio (o la inizializzo)
    const sigset_t *sig_set = init_sigset();

    // Setting effettivo della maschera di bloccaggio
    if (pthread_sigmask(SIG_BLOCK, sig_set, &old_sig_set) == -1){
	perror("Errore nel setting della maschera di bloccaggio.");
	goto cleanup;
    }

    // Invio del numero totale di blocchi
    uint32_t n_net = htonl(blocks->n);
    if (c_send(client_fd, (unsigned char *)&n_net, sizeof(uint32_t))) {
	fprintf(stderr, "Errore nell'invio del numero di blocchi.\n");
	goto cleanup;
    }	

    // Invio dei blocchi
    int blocks_sent = 0;
    while (blocks_sent < blocks->n){
	if (c_send(client_fd, (unsigned char *)&(blocks->array[blocks_sent]), sizeof(uint64_t))){
	    fprintf(stderr, "Errore nell'invio del blocco numero %d.\n", blocks_sent);
	    goto cleanup;
	}
	blocks_sent++;
    }

    // Invio della chiave di cifratura
    if (c_send(client_fd, (unsigned char *)&(k), sizeof(uint64_t))){
	fprintf(stderr, "Errore nell'invio della chiave di cifratura.\n");
	goto cleanup;
    }


    // Chiusura della funzione
    // Successo
    ret_val = 0;

    // Blocco cleanup
 cleanup:
    //Ripristino della maschera dei segnali
    pthread_sigmask(SIG_SETMASK, &old_sig_set, NULL);

    return ret_val;
}


int c_send(int client_fd, unsigned char *data_ptr, size_t data_size){

    // Contatori
    ssize_t sent = 0;
    int failures = 0;

    // Array per l'iterazione
    unsigned char *bytes = data_ptr;

    while (sent < data_size){
	ssize_t bytes_to_send = data_size - sent;
	ssize_t iteration_sent = send(client_fd, &(bytes[sent]), bytes_to_send, MSG_NOSIGNAL);
	
	if (iteration_sent > 0) {
	    sent += iteration_sent;
	    failures = 0; 
	} else if (iteration_sent < 0) {
	    if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Risorsa temporaneamente non disponibile, riprova più tardi
            sleep(1);
            continue;
	    }
	    perror("send failed");
	    return 1;
	} else {
	    // if iteration_sent == 0;
	    if (++failures == 10) return 1;
	    sleep(1);
	}
    }

    return 0;
}


int receive_ack(int client_fd){

    int ret_val = 1;

    // Allocazione del buffer per la ricezione di "ACK"
    char buffer[4];
    memset(buffer, 0, sizeof(buffer));

    // Ricezione del messaggio via socket
    ssize_t received = recv(client_fd, buffer, 3, 0);

    // Gestione casistiche di errore
    switch (received){
    case -1 :
	fprintf(stderr, "Errore: la ricezione dell'ack non è andata a buon fine.\n");
	break;
    case 0 :
	fprintf(stderr, "Errore: il server ha terminato la connessione.\n");
	break;
    default :
	buffer[received] = '\0';
	if (strcmp(buffer, "ACK") == 0) {
	    ret_val = 0;
	} else {
	    fprintf(stderr, "Errore: messaggio incorretto, ACK richiesto.\n");
	}
	break;
    }

    return ret_val;
}


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <time.h>

#include "functions.h"
#include "structs.h"
#include "globals.h"

#define PORT 1312


int manage_connections(SockQueue *queue, int l){

    // Variabili e strutture per i socket 
    int server_fd, sock_fd;
    socklen_t sock_len;
    struct sockaddr_in server_addr, sock_addr;

    // Creazione del socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
	perror("socket");
	return 1;
    }

    // Inizializzazione server_addr
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);    	
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
	perror("setsockopt");
	close(server_fd);
	return 1;
    }

    // Associazione file descriptor - indirizzo:porta
    if (bind(server_fd,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
	perror("bind");
	close(server_fd);
	return 1;
    }

    // Attivazione socket di ascolto
    if (listen(server_fd,l) < 0){
	perror("listen");
	close(server_fd);
	return 1;
    }

    // Ciclo di accettazione connessioni
    while (!shutdown_server_flag) {

	// Struttura che setta il timeout di select
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	// Creazione set di fd di ascolto
	fd_set read_fds;
	FD_ZERO(&read_fds); 
	FD_SET(server_fd,&read_fds);
		
	// Select
	int select_return_val = select(server_fd+1, &read_fds, NULL, NULL, &tv);

	// Gestione del valore di ritorno di select
	
	// Errore o segnale ricevuto
	if (select_return_val == -1){

	    // Controllo SIGINT
	    if(errno == EINTR){
		// Ritorno alla condzione di controllo del while per uscire dal loop
		continue;
	    }
	    
	    perror("select");
	    break;
	}

	// Nessuna richiesta di connessione
	else if (select_return_val == 0){
	    continue;  
	}

	// Richiesta di connessione ricevuta	
	else {

	    // Mettere maschera segnali
		
	    sock_len = sizeof(sock_addr);	    	
	    sock_fd = accept(server_fd,(struct sockaddr*)&sock_addr,&sock_len);

	    if (sock_fd < 0) {
		if (errno == EINTR) continue;
		perror("accept");
		continue;
	    }

	    // Lock
	    pthread_mutex_lock(&queue->mutex);

	    // Creazione di un nuovo nodo contenente il fd della nuova connessione
	    SockNode *new_node = (SockNode *)malloc(sizeof(SockNode));
	    new_node->fd = sock_fd;
	    new_node->next = NULL;

	    // Inserimento del nodo nella coda	
	    if (queue->tail == NULL){
		queue->head = queue->tail = new_node;
	    } else {
		queue->tail->next = new_node;
		queue->tail = new_node;
	    }

	    // Risveglia un worker
	    pthread_cond_signal(&queue->cond_not_empty);
	    pthread_mutex_unlock(&queue->mutex);

	    // Togliere maschera segnali
	}
    }


    // Attesa che tutti i workers siano idle
    pthread_mutex_lock(&idle_mutex);
    while (idle_workers < l) {
	pthread_cond_wait(&all_idle_cond, &idle_mutex);
    }
    pthread_mutex_unlock(&idle_mutex);


    return 0;
}

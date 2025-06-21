#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"

void sig_handler(int signum){
    if (signum == SIGINT || signum == SIGALRM || signum == SIGUSR1 || signum == SIGUSR2 || signum == SIGTERM){
        shutdown_server_flag = 1;
        fprintf(stderr, "Segnale %d ricevuto. Avvio dello spegnimento del server.\n", signum);
    }
}

int setup_signal_handling(){
    struct sigaction sa;

    // Pulisce la struttura sigaction per evitare valori residui
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask); // La maschera dei segnali di sa è vuota, 
							  // quindi nessun segnale è bloccato *mentre l'handler è in esecuzione*.
    sa.sa_flags = 0; 

    // Registra l'handler per SIGINT
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("sigaction for SIGINT");
        return 1;
    }

    // Registra l'handler per SIGALRM
    if(sigaction(SIGALRM, &sa, NULL) == -1){
        perror("sigaction for SIGALRM");
        return 1;
    }

    // Registra l'handler per SIGUSR1
    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction for SIGUSR1");
        return 1;
    }

    // Registra l'handler per SIGUSR2
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction for SIGUSR2");
        return 1;
    }

    // Registra l'handler per SIGTERM
    if(sigaction(SIGTERM, &sa, NULL) == -1){
        perror("sigaction for SIGTERM");
        return 1;
    }
    
    return 0;
}

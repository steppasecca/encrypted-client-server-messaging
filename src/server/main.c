#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h> 
#include <getopt.h>
#include <stdatomic.h>

#include "structs.h"
#include "functions.h"
#include "globals.h"

// Dichiarazione del set dei segnali da bloccare
sigset_t block_sigset;
atomic_flag mask_initialized = ATOMIC_FLAG_INIT;


// Variabili per il conteggio dei workers in attesa
volatile int idle_workers = 0;
pthread_mutex_t idle_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_idle_cond = PTHREAD_COND_INITIALIZER;


// Variabile per la gestione del segnale di stop 
volatile sig_atomic_t shutdown_server_flag = 0;


// Funzione di inizializzazione della maschera
const sigset_t * init_sigset(){

	// Controllo se la mask è già stata inizializzata
	if (!atomic_flag_test_and_set(&mask_initialized)){	
		// Setting del set block_sigset
		sigemptyset(&block_sigset);
		sigaddset(&block_sigset, SIGINT);
		sigaddset(&block_sigset, SIGALRM);
		sigaddset(&block_sigset, SIGUSR1);
		sigaddset(&block_sigset, SIGUSR2);
		sigaddset(&block_sigset, SIGTERM);
	}

	// Restituzione del puntatore del set inizializzato
	return &block_sigset;
}

void print_usage(char *program_name) {
	fprintf(stderr, "Utilizzo: %s [OPZIONI]\n", program_name);
	fprintf(stderr, "Client per la cifratura e l'invio di dati.\n\n");
	fprintf(stderr, "Opzioni:\n");
	fprintf(stderr, "  -o, --output <path>     Specifica il percorso del file di output (obbligatorio).\n");
	fprintf(stderr, "  -l, --<string> max_connections_arg    specifica il numero di connessioni parallele(obbligatorio).\n");
	fprintf(stderr, "  -p, --threads <num>    Specifica il numero di thread paralleli per la decifratura (obbligatorio).\n");
	fprintf(stderr, "  -h, --help             Mostra questo messaggio di aiuto ed esce.\n");
}
int main(int argc,char **argv){

	int ret_val = 1;

	// Inizializzazione sig_handlers
	if (setup_signal_handling()){
		goto cleanup;
	}

	// Dichiarazione delle variabili per immagazzinare gli argomenti
	char *output_path_arg = NULL;
	char *threads_string_arg = NULL;
	char *max_connections_arg = NULL;
	int show_help = 0;

	// Inizializzazione variabili per la gestione degli argomenti tramite getopt 
	int opt;
	int opt_index = 0;
	int parsing_error = 0;

	static struct option long_options[] = {
		{"output",   required_argument, 0, 'o'},
		{"threads", required_argument, 0, 'p'},
		{"max_connections", required_argument, 0, 'l'},
		{"help", 	no_argument		 , 0, 'h'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "o:p:l:h:", long_options, &opt_index)) != -1) {
		switch (opt) {
			case 'o':
				output_path_arg = optarg;
				break;
			case 'p':
				threads_string_arg = optarg;
				break;
			case 'l':
				max_connections_arg = optarg;
				break;
			case 'h':
				show_help = 1;
				break;
			case '?':
				if (optopt) {
					fprintf(stderr, "Errore: opzione sconosciuta o argomento mancante '-%c'\n", optopt);
				} else if (opt_index < sizeof(long_options) / sizeof(struct option) - 1 && long_options[opt_index].flag == 0) {
					fprintf(stderr, "Errore: opzione sconosciuta o argomento mancante --%s\n", long_options[opt_index].name);
				} else if (argv[optind - 1]) {
					fprintf(stderr, "Errore: opzione sconosciuta '%s'\n", argv[optind - 1]);
				} else {
					fprintf(stderr, "Errore: opzione sconosciuta o formato errato\n");
				}
				parsing_error = 1;
				break;
		}
	}

	//se show_help è true allora stampiamo le opzioni
	if(show_help){
		print_usage(argv[0]);
		return 0;
	}
	// Controllo presenza errori nel parsing degli argomenti ed esco
	if (parsing_error){
		return 1;
	}	    

	//  Controllo argomenti obbligatori mancanti
	//  -l è obbligatorio o opzionale?
	if (!output_path_arg || !threads_string_arg || !max_connections_arg) {
		fprintf(stderr, "Errore: argomenti obbligatori mancanti.\n");
		fprintf(stderr, "-o : %s.\n", output_path_arg ? output_path_arg : "NULL"); 
		fprintf(stderr, "-p : %s.\n", threads_string_arg ? threads_string_arg : "NULL");
		fprintf(stderr, "-l : %s.\n", max_connections_arg ? max_connections_arg : "NULL"); 
		return 1;
	}

	// Elaborazione degli inputs

	// Numero di threads paralleli:    
	char *endptr_p;
	errno = 0;
	long p_long = (int)strtol(threads_string_arg, &endptr_p, 10);
	if (endptr_p == threads_string_arg || *endptr_p != '\0'){
		fprintf(stderr,"Errore: caratteri non numerici rilevati.\n");
		return 1;
	}
	if (errno == ERANGE){
		fprintf(stderr, "Errore: overflow nella lettura di thread_string_arg.\n");
		return 1;
	}
	if (p_long <= 0){
		fprintf(stderr, "Errore: numero di parallelismo minore o uguale a 0.\n");
		return 1;
	}
	int p = (int)p_long;

	// Numero massimo di connesioni consentite
	char *endptr_l;
	errno = 0;
	long l_long = (int)strtol(max_connections_arg, &endptr_l, 10);
	if (endptr_l == max_connections_arg || *endptr_l != '\0'){
		fprintf(stderr,"Errore: caratteri non numerici rilevati.\n");
		return 1;
	}
	if (errno == ERANGE){
		fprintf(stderr, "Errore: overflow nella lettura di max_connections_arg.\n");
		return 1;
	}
	if (l_long <= 0){
		fprintf(stderr, "Errore: numero di connessioni consentite minore o uguale a 0.\n");
		return 1;
	}
	int l = (int)l_long;

	// Allocazione risorse principali

	// Coda per le connessioni in attesa di essere gestite tramite workers
	SockQueue *queue = (SockQueue *)malloc(sizeof(SockQueue));
	if (queue == NULL){
		fprintf(stderr, "Errore nell'allocazione di queue\n");
		goto cleanup;
	}
	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->cond_not_empty, NULL);
	queue->head = queue->tail = NULL;

	// Buffer condiviso per la raccolta dei blocchi ricevuti e decrittati
	OutputBuffer *output_buffer = (OutputBuffer *)malloc(sizeof(OutputBuffer));
	if (output_buffer == NULL){
		fprintf(stderr,"Errore nell'allocazione di output_buffer\n");
		goto cleanup;
	}
	pthread_mutex_init(&output_buffer->mutex, NULL);

	// Primo nodo della lista output_buffer
	BlockNode *output_buffer_head = (BlockNode *)malloc(sizeof(BlockNode));
	if (output_buffer_head == NULL) {
		fprintf(stderr,"Errore nell'allocazione di output_buffer_head\n");
		goto cleanup;
	}
	memset(&output_buffer_head->block, 0, sizeof(uint64_t));
	output_buffer_head->next = NULL;
	output_buffer->head = output_buffer->tail = output_buffer_head;


	// Struttura per il passaggio degli argomenti ai workers
	WorkerArgs *worker_args = (WorkerArgs *)malloc(sizeof(WorkerArgs));
	if (worker_args == NULL){
		fprintf(stderr,"Errore nell'allocazione di worker_args\n");
		goto cleanup;
	}
	worker_args->queue = queue;
	worker_args->output_buffer = output_buffer;
	worker_args->p = p;
	worker_args->l = l;


	// Inizializzazione dei thread workers
	if (init_workers(worker_args, l)){
		fprintf(stderr,"Errore in init_workers\n");
		goto cleanup;
	}

	// Apertura del socket d'ascolto e gestione delle connessioni in arrivo
	if (manage_connections(queue, l)){
		fprintf(stderr,"Errore in manage_connections\n");
		goto cleanup;
	}

	// Scrittura del file di output
	if (write_file(output_path_arg,output_buffer)){
		fprintf(stderr,"Errore in write_file\n");
		goto cleanup;
	}

	// Chiusura funzione
	// Successo
	ret_val = 0;

cleanup:

	if (worker_args != NULL) free(worker_args);

	if (output_buffer != NULL) {
		BlockNode *b_it1 = output_buffer->head;
		BlockNode *b_it2;
		while (b_it1 != NULL) {
			b_it2 = b_it1;
			free(b_it1);
			b_it1 = b_it2->next;
		}
		pthread_mutex_destroy(&output_buffer->mutex);
		free(output_buffer);
	}

	if (queue != NULL) {
		SockNode *s_it1 = queue->head;
		SockNode *s_it2;
		while (s_it1 != NULL) {
			s_it2 = s_it1;
			free(s_it1);
			s_it1 = s_it2->next;
		}
		pthread_mutex_destroy(&queue->mutex);
		pthread_cond_destroy(&queue->cond_not_empty);
		free(queue);
	}

	return ret_val;
}

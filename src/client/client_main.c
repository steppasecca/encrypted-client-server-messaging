#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h> 
#include <getopt.h>
#include <signal.h>
#include <stdatomic.h>

#include "structs.h"
#include "functions.h"

// Dichiarazione del set dei segnali da bloccare
sigset_t block_sigset;
atomic_flag mask_initialized = ATOMIC_FLAG_INIT;

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

//funzione per stampare il messaggio delle opzioni di utilizzo
//qui vorrei trovare un modo per non scrivere le righe troppo lunghe
//forse banalmente andando a capo
void print_usage(char *program_name) {
    fprintf(stderr, "Utilizzo: %s [OPZIONI]\n", program_name);
    fprintf(stderr, "Client per la cifratura e l'invio di dati.\n\n");
    fprintf(stderr, "Opzioni:\n");
    fprintf(stderr, "  -i, --input <path>     Specifica il percorso del file di input (obbligatorio).\n");
    fprintf(stderr, "  -k, --key <string>     Specifica la chiave di cifratura (obbligatorio).\n");
    fprintf(stderr, "  -p, --threads <num>    Specifica il numero di thread paralleli per la cifratura (obbligatorio).\n");
    fprintf(stderr, "  -s, --address [host:port] Specifica l'indirizzo del socket di destinazione (opzionale, default: 127.0.0.1:1312).\n");
    fprintf(stderr, "  -h, --help             Mostra questo messaggio di aiuto ed esce.\n");
}

int main(int argc, char **argv){

    // Dichiarazione delle variabili per immagazzinare gli argomenti
    char *input_path_arg = NULL;
    char *key_string_arg = NULL;
    char *threads_string_arg = NULL;
    char *socket_address_arg = NULL;
	int show_help = 0; //flag per indicare se mostrare le opzioni di help

    // Inizializzazione variabili per la gestione degli argomenti tramite getopt
    int opt;
    int opt_index = 0;
    int parsing_error = 0;
    
    static struct option long_options[] = {
        {"input",   required_argument, 0, 'i'},
        {"key",     required_argument, 0, 'k'},
        {"threads", required_argument, 0, 'p'},
        {"address", optional_argument, 0, 's'},
		{"help", 	no_argument		 , 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "i:k:p:s:h:", long_options, &opt_index)) != -1) {
        switch (opt) {
	case 'i':
	    input_path_arg = optarg;
	    break;
	case 'k':
	    key_string_arg = optarg;
	    break;
	case 'p':
	    threads_string_arg = optarg;
	    break;
	case 's':
	    socket_address_arg = optarg;
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
    if (!input_path_arg || !key_string_arg || !threads_string_arg) {
        fprintf(stderr, "Errore: argomenti obbligatori mancanti.\n");
		fprintf(stderr, "-i : %s.\n", input_path_arg ? input_path_arg : "NULL"); 
		fprintf(stderr, "-k : %s.\n", key_string_arg ? key_string_arg : "NULL");
		fprintf(stderr, "-p : %s.\n", threads_string_arg ? threads_string_arg : "NULL");
		print_usage(argv[0]); //stampiamo il menu di opzioni in caso di argomenti
        return 1;
    }


    // Elaborazione degli inputs

    // Key:
    uint64_t k = 0;
    size_t key_len = strlen(key_string_arg);
    if (key_len < sizeof(uint64_t)){
	fprintf(stderr, "Errore: lunghezza chiave insufficiente.\n");
	return 1;
    }
    // Copia dei primi 8 byte
    memcpy(&k, key_string_arg, sizeof(uint64_t));
    // Nota di Gemini sulla endianness: La rappresentazione dei byte all'interno di 'k' dipenderà
    // dall'endianness del sistema. Se la chiave deve avere un significato specifico
    // indipendente dall'endianness, dovresti usare funzioni come htobe64() o htonl()
    // e htonll() per la portabilità.


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

    
    // Indirizzo socket di destinazione
    char *target_address = NULL;
    if (socket_address_arg){
	errno = 0;
	target_address = strdup(socket_address_arg);
	if (target_address == NULL){
	    perror("Errore: allocazione target_address fallita.");
	    return 1;
    	}
    }

	
    // Istanziazione della struttura principale in cui inserire e gestire i blocchi
    Blocks blocks= { .n=0 , .array=NULL };

    
    if (load_file(input_path_arg, &blocks)){
        // Gestione errori
	free(blocks.array);
	if (target_address) free(target_address);
        return 1;
    }

    // Gestione cifratura e thread  
    if (manage_encryption(p, k, &blocks)){
	// Gestione errori
	free(blocks.array);
	if (target_address) free(target_address);
	return 1;
    }

    // Connessione e invio al server
    if (connection(target_address, &blocks, k)){
	// Gestione errori
	free(blocks.array);
	if (target_address) free(target_address);
	return 1;
    }

    // Deallocazione
    free(blocks.array);
    if (target_address) free(target_address);

    return 0;
}


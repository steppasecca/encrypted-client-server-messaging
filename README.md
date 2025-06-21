# encrypted-client-server-messaging
A simple C client-server program for encrypted messagging for a homework of operating systems course in sapienza university of rome

Sviluppare un applicazione client-server che permetta ad un processo client di inviare ad un processo server un dato cifrato. Il processo server dovrà decifrare il dato ricevuto e salvarlo su file. I processi client e server dovranno comunicare mediante socket 

Specifica processo Client
L’applicazione client riceve come input

    il nome di un file (potrebbe essere un file di testo o binario, ma al fine della verifica del funzionamento sarà utile un file di testo)
    una chiave K di lunghezza 64 bit (rappresentabile mediante un intero di tipo unsigned long long int)
    un intero p che determina il grado di parallelismo desiderato nel calcolo della cifratura
    l’indirizzo IP del server ed il numero di porta su cui il server è in ascolto

Il client legge il file di input e ne estrare il testo F di cui ne determina la lunghezza L.  Lo divide quindi in n blocchi Bi di lunghezza  64 bit.
 
F=B1B2 … Bn

L’ultimo blocco Bn potrebbe contenere meno di 64 bit. In tal caso, il blocco Bn deve essere riempito con un padding di caratteri scelto, ad esempio ‘/0’.

Per ogni blocco Bi il client calcola il bitwise XOR con la chiave K ottenendo quindi il blocco cifrato Ci= Bi XOR K.

Il testo cifrato C è dato dalla concatenazione degli n blocchi, nell’ordine C1C2…Cn e deve essere inviato al server insieme alla lunghezza L del file originale e alla chiave K. Esempio di struttura del messaggio inviato dal client al server [C1C2…Cn, L, K]

Il processo client, per terminare, attende un messaggio di avvenuta ricezione del messaggio inviato  da parte del server (acknowledgment)

Requisiti del processo Client

    Il client deve utilizzare al più p thread per eseguire le operazioni di cifratura in parallelo.
    Il client deve gestire correttamente la chiusura della connessione da parte del server.
    Il processo client, quando effettua le operazioni di cifratura e di invio non deve essere interrotto da (SIGINT, SIGALRM, SIGUSR1, SIGUSR2, SIGTERM).
    L’uso di eventuali strutture dati condivise tra thread deve essere progettato in modo da evitare race conditions.

Specifica del processo Server
L’applicazione server riceve come input

    un intero p che determina il grado di parallelismo desiderato nel calcolo della decifratura
    una stringa s che costituisce un prefisso per il nome dei file di output
    un intero l che indica il numero di connessioni che possono essere concorrentemente gestite dal server.

Il processo server si pone in ascolto su di una specifica porta ed accetta richeste di connessione da più processi client.
Il server riceve dal client un messaggio contenente un testo cifrato C, la lunghezza del testo cifrato L e la chiave di cifratura K (64 bit). Il server estrae queste informazioni dal messaggio, divide C in n blocchi di dimensione 64 bit e, facendo uso di p thread (p<=n) decifra il messaggio calcolando il bitwise XOR tra K ed ogni blocco Ci per ottenere Bi = Ci XOR K.

I thread memorizzano I blocchi decifrati in un buffer condiviso. Dopo che il messaggio è stato completamente decifrato e ricostruito concatenando gli n blocchi Bi, viene salvano su di un file il cui nome inizia con il prefisso s.

Dopo aver terminato l’operazione di decifratura, ma prima di aver scritto sul file, il processo invia l’acknowledgement al client e chiude la connessione con il client.

Requisiti del server

    un thread del processo server che sta decifrando un messaggio C non deve essere interrotto da segnali (SIGINT, SIGALRM, SIGUSR1, SIGUSR2, SIGTERM).
    Un thread del processo server che sta scrivendo il testo decifrato su disco non deve essere interrotto dal segnali (SIGINT, SIGALRM, SIGUSR1, SIGUSR2, SIGTERM)
    Il buffer condiviso per la memorizzazione del testo decifrato deve essere allocato dinamicamente, gestito come una lista concatenata di blocchi, e le race condition devono essere evitate.

Requisiti generali

    I parametri delle applicazioni client e server devono essere passata come parametri da linea di commando, oppure come valori di variabili di ambiente
    Le applicazioni client e server devono gestire correttamente gli errori nei parametri di input, ma anche tutti i possibili errori di chiamata a funzioni e system call

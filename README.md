# Chatterbox
Progetto sistemi operativi 2017/2018 @UNIPI<br>
Server chat multithread

# Descrizione generale
Lo scopo del progetto è lo sviluppo in C di un server concorrente che implementa una chat. Gli utenti della chat possono scambiarsi messaggi testuali e/o files collegandosi al server con un programma client.
Un messaggio testuale è una stringa non vuota avente una lunghezza massima stabilita nel file di configurazione del server. Un file di qualsiasi tipo può essere scambiato tra due client purchè la size non sia superiore a quanto stabilito nel file di configurazione del server.
Ogni utente della chat è identificato univocamente da un nickname alfanumerico.
L’utente si connette al server, invia richieste e riceve risposte utilizzando un client la cui struttura base è fornita (vedere il file client.c nel kit del progetto). Uno o più processi client comunicano con il server per inviare richieste di vario di tipo.<br> Le richieste gestite dal server sono almeno le seguenti:
1. registrazione di un nuovo utente (ossia registrazione di un nuovo nickname) (REGISTER OP);
2. richiesta di connessione per un utente già registrato (CONNECT OP);
3. invio di un messaggio testuale (POSTTXT OP) ad un nickname;
4. invio di un messaggio testuale a tutti gli utenti registrati (POSTTEXTALL OP);
5. richiesta di invio di un file (POSTFILE OP) ad un nickname;
6. richiesta di recupero di un file inviato da un altro nickname (GETFILE OP);
7. richiesta di recupero degli ultimi messaggi inviati al client (GETPREVMSGS OP);
8. richiesta della lista di tutti i nickname connessi (USRLIST OP);
9. richiesta di deregistrazione di un nickname (UNREGISTER OP);
10. richiesta di disconnessione del client (DISCONNECT OP).


Per ogni messaggio di richiesta il server risponde al client con un messaggio di esito dell’operazione richiesta e con l’eventuale risposta. Ad esempio, se si vuole inviare un messaggio ad un utente non esistente (cioè il cui nickname non è stato mai registrato), il server risponderà con un messaggio di errore opportunamento codificato (vedere file ops.h) oppure risponderà con un messaggio di successo (codificato con il codice OP OK – vedere il file ops.h contenente i codici delle operazioni di richiesta e di risposta).<br>

La comunicazione tra client e server avviene tramite socket di tipo AF UNIX. Il path da utilizzare per la creazione del socket AF UNIX da parte del server, deve essere specificato nel file di configurazione utilizzando l’opzione UnixPath.<br>

Nello sviluppo del codice del server, si dovrà tenere in considerazione che il server deve essere in grado di gestire efficientemente fino ad alcune centinaia di connessioni di client contemporanee senza che questo comporti un rallentamento evidente nell’invio e ricezione dei messaggi/files dei clients. Inoltre il sistema deve essere progettato per gestire alcune decine di migliaia di utenti registrati alla chat (questo aspetto ha impatto sulle strutture dati utilizzate per memorizzare le informazioni). <br>

Il server al suo interno implementa un sistema multi-threaded in grado di gestire concorrentemente sia nuove connessioni da nuovi client che gestire le richieste su connessioni già stabilite. Un client può sia inviare una sola richiesta per connessione oppure, inviare più richieste sulla stessa connessione (ad esempio: postare un messaggio ad un utente, inviare un file ad un nickname, inviare un messaggio a tutti gli utenti, recuperare file inviati da altri client, etc...). Il numero massimo di connessioni concorrenti che il server chatterbox gestisce è stabilito dall’opzione di configurazione MaxConnections. Se tale numero viene superato, dovrà essere ritornato al client un opportuno messaggio di errore (vedere la codifiche dei messaggi di errore nel file ops.h).<br>

Il numero di thread utilizzati per gestire le connessioni è specificato nel file di configurazione con l’opzioni ThreadsInPool. Tale numero specifica la dimensione di un pool di threads, sempre attivi per tutta la durata dell’esecuzione del server e che il server chatterbox utilizzerà per gestire le connessioni dei client. Tale valore non può essere 0.<br>

Un thread appartenente al pool gestisce una sola richiesta alla volta inviata da uno dei client. Non appena conclude la gestione della richiesta, controlla se ci sono altre richieste da servire dalla coda delle richieste ed in caso affermativo ne prende una da gestire. Se non ci sono richiesta da servire, il thread si mette in attesa di essere svegliato per gestire una nuova richiesta. Non appena il server accetta una nuova connessione da un client che ha inviato una richiesta di CONNECT, il server invia al client la lista di tutti gli utenti connessi in quel determinato istante (il formato della lista inviata dal server al client è lo stesso del messaggio di risposta alla richiesta USRLIST OP).<br>

Il server può essere terminato in ogni momento della sua esecuzione inviando un segnale di SIGINT, SIGTERM o SIGQUIT.

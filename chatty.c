/**
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 * @file chatty.c
 * @brief File principale del server chatterbox
 * 
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

/* inserire gli altri include che servono */

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <fcntl.h>
#include "connections.h"
#include "ops.h"
#include "queue.h"
#include "parser.h"
#include "icl_hash.h"
#include "user.h"
#include "util.h"
#include "stats.h"

#define NBUCKETS 1024 // Dimensione tabella hash 

/*********************************** Variabili globali ***********************************/ 

/* Struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
 */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };

/* Struttura che memorizza le configurazioni del server, struct serverConfiguration
 * e' definita in parser.h.
 *
 */
struct serverConf configuration = { {'\0', '\0', '\0', 0, 0, 0, 0, 0} };

// Bitmap segnali
sigset_t sigset;

// Coda
Queue_t *q; 

pthread_t *threadPool, sigTread;

// Strutture dati 
users_db_t *users_db = NULL;

// Variabile per far terminare il server
static volatile sig_atomic_t stop = 0;

// Insime dei fd attivi
fd_set set;

static pthread_mutex_t mtx_set = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_stats = PTHREAD_MUTEX_INITIALIZER;

/********************************* Funzioni  *********************************/

static void usage(const char *progname){
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

/**
 * @function sigHandler
 * @brief Funzione eseguita dal thread che gestisce i segnali
 *
 * @param arg
 *
 * @return null
 */
void *sigHandler(void *arg) {
    int sig;
    fprintf(stdout, "\tThread sigHandle start\n");

   while(!stop){
	    int r = sigwait(&sigset, &sig);
	    if (r != 0) {
	        errno = r;
	        perror("sigwait");
	        pthread_exit(NULL);
	    }

        if(sig == SIGINT || sig == SIGTERM || sig == SIGQUIT){
	        fprintf(stdout, "\t[SigWaitThread] Ricevuto segnale di chiusura server!\n");
            stop = 1;
        }
        else if(sig == SIGUSR1){   //BUG stampa
            fprintf(stdout, "\t[SigWaitThread]Ricevuto segnale di stampa statistiche!\n");
            FILE * statsFile = fopen(configuration.StatFileName, "a");
            if(statsFile == NULL){
                perror("fopen");
            }
            if(printStats(statsFile) != 0){
                fprintf(stderr, "\t[SigWaitThread]Scrittura file statistiche fallita\n");
            }
            fprintf(stdout, "\t[SigWaitThread]Scrittura file statistiche completata\n");
            fclose(statsFile);
        }
    }
    return NULL;
}

/**
 * @function setDir
 * @brief Aggiunge DirName al path filePath
 *
 * @param filePath     
 *
 * @return DirName concatenato con filePath
 */
char * setDir(char *filePath){
    char *dir; //DirName + filePath 
    char *tmp;

    dir = Malloc( (strlen(configuration.DirName) + 
                    strlen(filePath) + 2)
                    * sizeof(char));
            
    // Copio dirName in dir
    strncpy(dir, configuration.DirName, strlen(configuration.DirName) + 1);
    tmp = strstr(filePath, "/");
    if(tmp == NULL){
        //Forma del tipo "file"
        strncat(dir, "/", 1);
        strncat(dir, filePath, strlen(filePath) + 1);
    }else{ //Forma del tipo "./file"
        strncat(dir, tmp, strlen(tmp) + 1);
    }

    // dir = DirName/file.*
    return dir;
}


/**
 * @function register_op
 * @brief Gestisce la richiesta di registrazione di un nickname
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int register_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    message_t ack;             
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tREGISTER_OP: %s\n", sender);
    
    // Registro il sender 
    int r = register_user(users_db, sender);        
    if (r == 0){   //Registrazione completata
        MUTEX_BLOCK(mtx_stats, {chattyStats.nusers++;});
        fprintf(stdout, "\t\t%s registrato\n", sender);
            
        // Connetto l'utente
        if (connect_user(users_db, sender, client_fd) < 0){ 
            fprintf(stderr, "\t\tOP_FAIL (Connessione utente fallita)\n");
            if(setSendAck(ack.hdr, OP_FAIL, client_fd) == -1) return -1;
        }

        else{ // Utente connesso
            MUTEX_BLOCK(mtx_stats, {chattyStats.nonline++;});
            fprintf(stdout, "\t\t%s connesso\n", sender);
                
            char *users_online; 
            
            // Recupero e invio la lista degli utenti online
            int len = get_users_online(users_db, &users_online);  
            setHeader(&(ack.hdr), OP_OK, "");
            setData(&(ack.data), "server", users_online, len * (MAX_NAME_LENGTH + 1)); 
            if(sendRequest(client_fd,&ack) <= 0){
                fprintf(stderr, "\t\tErrore invio lista utenti online\n");
                free(users_online);
                return -1;
            }
            free(users_online);
            fprintf(stdout, "\t\tUtenti online inviati correttamente\n");
        }
    }else if(r == -1) { // Nome utente già registrato
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "OP_NICK_ALREADY\n");
        if(setSendAck(ack.hdr, OP_NICK_ALREADY, client_fd) == -1) return -1;
       
    }else if(r == -2){  // Registrazione fallita
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "OP_FAIL (registrazione utente fallita)\n");
       if(setSendAck(ack.hdr, OP_FAIL, client_fd) == -1) return -1;
    }

    return 0;
}

/**
 * @function connect_op
 * @brief Gestisce la richiesta di connessione di un client 
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int connect_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    message_t ack; 
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tCONNECT_OP: %s\n", sender);
    fprintf(stdout, "\t\tConnessione...\n");

    // Connetto il sender
    int ret = connect_user(users_db, sender, client_fd);
    if (ret == 0){   // Sender connesso
        MUTEX_BLOCK(mtx_stats, {chattyStats.nonline++;});
        fprintf(stdout, "\t\t%s Connesso\n", sender);
        char *users_online;

        // Recupero e invio la lista degli utenti online
        int len = get_users_online(users_db, &users_online);
        setHeader(&(ack.hdr), OP_OK, "");
        setData(&(ack.data), "server", users_online, len * (MAX_NAME_LENGTH + 1)); 
        if(sendRequest(client_fd,&ack) <= 0){
            fprintf(stderr, "\t\tErrore invio utenti online\n");
            free(users_online);
            return -1;
        }
        free(users_online);
        fprintf(stdout, "\t\tUtenti online inviati correttamente\n");
    }
    else if(ret == -2){  // Sender non registrato
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stdout, "\t\t\tOP_NICK_UNKNOWN\n");
        if(setSendAck(ack.hdr, OP_NICK_UNKNOWN, client_fd) == -1) return -1;
    }
    else if(ret == -1 || ret == -3){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stdout, "\t\t\tOP_FAIL (connessione fallita)\n");
        if(setSendAck(ack.hdr, OP_FAIL, client_fd) == -1) return -1;
    }
    return 0;
}

/**
 * @function posttxt_op
 * @brief Gestisce la richiesta di invio di un messaggio testuale ad un nickname 
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int posttxt_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    char *receiver = msg_receved.data.hdr.receiver;
    int len = msg_receved.data.hdr.len;
    message_t ack; 
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tPOSTTXT_OP: %s\n", sender);

    // Controllo la lunghezza del messaggio
    if (len > configuration.MaxMsgSize){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "\t\tOP_MSG_TOOLONG\n"); 
        setSendAck(ack.hdr, OP_MSG_TOOLONG, client_fd);
        return -1;
    }

    // Ottengo la struttura dati del receiver
    user_t *user = get_user(users_db, receiver);
    if(user == NULL){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "\t\tOP_FAIL (Recupero utente dalla tabella hash)\n");
        setSendAck(ack.hdr, OP_FAIL, client_fd);
        return -1;
    }
    int fd_rcv = user->fd;
    msg_receved.hdr.op = TXT_MESSAGE;
    
    // Copio il messaggio ricevuto dal client
    message_t *tosend = copyMessage(&msg_receved);   
    if(user->fd > 0){ //Receiver connesso e registrato
        fprintf(stdout, "\t\t%s è online, gli invio il messaggio\n", receiver);

        // Invio del messaggio 
        if(sendRequest(fd_rcv, tosend) <= 0){                   
            MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
            setSendAck(ack.hdr, OP_FAIL, client_fd);
            freeMessage(tosend);
            return -1;
        }
        MUTEX_BLOCK(mtx_stats, {chattyStats.nnotdelivered--;});
        MUTEX_BLOCK(mtx_stats, {chattyStats.ndelivered++;});
    }
    
    // Inserisco il messaggio nella history dell'utente
    if(insertMsg(user->history, tosend) < 0){                
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "\t\tOP_FAIL (Inserimento messaggio nella history)\n");
        setSendAck(ack.hdr, OP_FAIL, client_fd);
        freeMessage(tosend);
        return -1;
    }
    MUTEX_BLOCK(mtx_stats, {chattyStats.nnotdelivered++;});
    fprintf(stdout, "\t\tInserimento messaggio nella history completato\n");

    if(setSendAck(ack.hdr, OP_OK, client_fd) == -1) return -1;
    return 0;
}

/**
 * @function posttxtall_op
 * @brief Gestisce l'invio di un messaggio testuale a tutti gli utenti registrati  
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int posttxtall_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    int len = msg_receved.data.hdr.len;
    message_t ack; 
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tPOSTTXTALL_OP: %s\n", sender);
    // Controllo lunghezza del messaggio 
    if (len > configuration.MaxMsgSize){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        printf("\t\tOP_MSG_TOOLONG\n"); 
        setSendAck(ack.hdr, OP_MSG_TOOLONG, client_fd);
        return -1;
    }

    user_t *user; 
    // Scorro in mutua esclusione la tabella hash
    icl_hash_foreach_mutex(users_db->db, user, {
        msg_receved.hdr.op = TXT_MESSAGE;
        message_t *tosend = copyMessage(&msg_receved);                     

        // Controllo per non inviare il messaggio a chi ha fatto richiesta 
        if(strncmp(user->name, sender, MAX_NAME_LENGTH+1)) {
            // Controllo se l'utente è online
            if(user->fd != -1){
                if(sendRequest(user->fd, tosend) <= 0) {
                    fprintf(stderr,"\t\tInvio messaggio a %s fallito\n", user->name);
                } else {
                    MUTEX_BLOCK(mtx_stats, {chattyStats.nnotdelivered--;});
                    MUTEX_BLOCK(mtx_stats, {chattyStats.ndelivered++;});
                }
            }

            //Inserisco il messaggio nella history
            if(insertMsg(user->history, tosend) < 0){
                fprintf(stderr, "\t\tInserimento messaggio nella history fallito\n");
            }
            MUTEX_BLOCK(mtx_stats, {chattyStats.nnotdelivered++;});
        }else{
            freeMessage(tosend);
        }
    })

    if(setSendAck(ack.hdr, OP_OK, client_fd) == -1) return -1;
    return 0;
}

/**
 * @function postfile_op
 * @brief Gestisce la richiesta di invio di un file ad un nickname
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int postfile_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    char *receiver = msg_receved.data.hdr.receiver;
    char *dir;
    message_t ack; 
    message_data_t file;
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tPOSTFILE_OP: %s\n", sender);
    if(readData(client_fd, &file) <= 0){                            
        printf("\t\tErrore readData\n");
        return -1;
    }

    // Controllo grandezza file
    if( (file.hdr.len)/1024 > configuration.MaxFileSize){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr,"\t\tOP_MSG_TOOLONG\n");
        setSendAck(ack.hdr, OP_MSG_TOOLONG, client_fd);
        free(file.buf);
        return -1;
    }
    
    // Ricostruisco l'intero path del file  Esempio: /tmp/chatty/file.*   
    dir = setDir(msg_receved.data.buf);          
    //Apro il file in sola scrittura, se non esiste lo creo 
    FILE *fd_f;
    fd_f = fopen(dir, "w");
    if (fd_f == NULL){
        fprintf(stderr, "\t\tErrore apertura file\n");
        free(file.buf);
        free(dir);
        return -1;
    }
    free(dir);

    int len_file = file.hdr.len;
    
    // Scrivo il file controllando che abbia scritto tuto 
    if(fwrite(file.buf, sizeof(char), len_file, fd_f) != len_file){
                perror("fwrite");
                fclose(fd_f);
                free(file.buf);
                return -1;
    }
    // Scrittura del file completata

    free(file.buf);
    fclose(fd_f);

    // Ottengo la struttura del receiver
    user_t *user = get_user(users_db, receiver);
    if(user == NULL){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        printf("\t\tOP_FAIL (Recupero utente dalla tabella hash)\n");
        setSendAck(ack.hdr, OP_FAIL, client_fd);
        return -1;
    }
    int fd_rcv = user->fd;

    msg_receved.hdr.op = FILE_MESSAGE;
    // Copio il messaggio ricevuto del client    
    message_t *tosend = copyMessage(&msg_receved);

    if(fd_rcv > 0){ //Receiver connesso e registrato
        // Invio messaggio al ricevente per avvertirlo che c'è un file a lui destinato
        if(sendRequest(fd_rcv, tosend) <= 0){
            MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
            setSendAck(ack.hdr, OP_FAIL, client_fd);
            freeMessage(tosend);
            return -1;
        }
        MUTEX_BLOCK(mtx_stats, {chattyStats.nfilenotdelivered--;});
        MUTEX_BLOCK(mtx_stats, {chattyStats.nfiledelivered++;});
    }

    // Inserisco il messaggio nella history
    if (insertMsg(user->history, tosend) < 0){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        printf("\t\tOP_FAIL (Inserimento file nella history)\n");
        setSendAck(ack.hdr, OP_FAIL, client_fd);
        freeMessage(tosend);
        return -1;
    }
    MUTEX_BLOCK(mtx_stats, {chattyStats.nfilenotdelivered++;});
    printf("\t\tInserimento history COMPLETATO\n");

    if(setSendAck(ack.hdr, OP_OK, client_fd) == -1) return -1;
    return 0;
}

/**
 * @function getfile_op
 * @brief Gestisce la richiesta di recupero di un file inviato da un altro nickname  
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int getfile_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    message_t ack; //Messaggio di risposta
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tGETFILE_OP: %s\n", sender);
    // Ricostruisco l'intero path del file  Esempio: /tmp/chatty/file.*       
    char *dir = setDir(msg_receved.data.buf);
        
    // Apro il file in sola lettura
    int fd = open(dir, O_RDONLY);
    if (fd < 0){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "\t\tOP_NO_SUCH_FILE\n");
        setSendAck(ack.hdr, OP_NO_SUCH_FILE, client_fd);
        free(dir);
        return -1;
    }

    // Ricavo informazioni sul file
    struct stat st;
    if(stat(dir, &st) == -1){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr, "\t\tOP_NO_SUCH_FILE\n");    
        setSendAck(ack.hdr, OP_NO_SUCH_FILE, client_fd);
        free(dir);
        return -1;
    }
    free(dir);
    
    // Controllo dimensione del file
    if(st.st_size > configuration.MaxFileSize){
        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
        fprintf(stderr,"\t\tOP_MSG_TOOLONG\n");
        setSendAck(ack.hdr, OP_MSG_TOOLONG, client_fd);
        return -1;
    }

    // Mappiamo il file da spedire in memoria
    char *mappedfile = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mappedfile == MAP_FAILED) {
		perror("mmap");
		fprintf(stderr, "\t\tERRORE: mappando il file in memoria\n");
		close(fd);
		return -1;
	}
    close(fd);

    message_t tosend;
    setHeader(&(tosend.hdr), OP_OK, "");
    setData(&(tosend.data), "server", mappedfile, st.st_size);   
    if(sendRequest(client_fd, &tosend) <= 0){
        fprintf(stderr,"\t\tErrore invio file\n"); 
        munmap(mappedfile, st.st_size);
        return -1;
    }
    munmap(mappedfile, st.st_size);
    return 0;
}

/**
 * @function getprevmsgs_op
 * @brief Gestisce la richiesta di recupero degli ultimi messaggi inviati al client  
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int getprevmsgs_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    message_t ack; 
    memset(&ack, 0, sizeof(message_t));
    fprintf(stdout, "\t\tGETPREVMSGS_OP: %s\n", sender);
        
    // Recupero la history del sender
    history_t *history = history_sender(users_db, sender);
    if (history != NULL){
        message_t **msgs;

        //Recupero i messaggi della history
        int n_msg = outMsg(history, &msgs); 
        if(n_msg >= 0){ 
            // Invio il numero di messaggi contenuti nella history
            setHeader(&(ack.hdr), OP_OK, "");
            setData(&(ack.data), "server",(char *) &n_msg, sizeof(int));
            if(sendRequest(client_fd, &ack) <= 0){
                fprintf(stderr, "\t\tErrore invio n. messaggi history\n");
                free(msgs);
                return -1;
            }

            // Sono presenti messaggi nella history
            if(n_msg > 0){
                int error = 0;
                printf("\t\tInvio messaggi in corso...\n");
                // Itero sull'array che contiene i messaggi 
                for(int i = 0; i < n_msg; i++){
                    message_t *msg = msgs[i];
                    
                    // Aggiorno le statistiche in base al messaggio letto 
                    if(msg->hdr.op == TXT_MESSAGE){
                        MUTEX_BLOCK(mtx_stats,{ chattyStats.nnotdelivered--;
                                    chattyStats.ndelivered++; }
                                    );
                    }
                    else{
                        MUTEX_BLOCK(mtx_stats,{ chattyStats.nfilenotdelivered--;
                                    chattyStats.nfiledelivered++; }
                                    );
                        }
                    int ret = sendRequest(client_fd, msg); // Invio messaggio
                    freeMessage(msg);
                    if(ret <= 0){
                        error++; 
                    } 
                }
                
                free(msgs);
                if(error > 0){
                    fprintf(stderr, "\t\tErrore invio messaggi\n");
                    return -1;
                } 
            }else{
                fprintf(stdout, "\t\tNon ci sono messaggi da leggere\n");
            }
            return 0; 
        }

    }
    MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
    fprintf(stdout, "\t\tOP_FAIL (Recupero history fallito)\n");
    setSendAck(ack.hdr, OP_FAIL, client_fd);
    return -1;
}

/**
 * @function usrlist_op
 * @brief Gestisce la richiesta della lista di tutti i nickname connessi 
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int usrlist_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    char *users_online;
    message_t ack; 
    memset(&ack, 0, sizeof(message_t));

    fprintf(stdout, "\t\tUSRLIST_OP: %s\n", sender);

    // Recupero la lista degli utenti online
    int n = get_users_online(users_db, &users_online);
    if(n > 0){
        setHeader(&(ack.hdr), OP_OK, "");
        setData(&(ack.data), "server", users_online, n * (MAX_NAME_LENGTH + 1)); 
        if (sendRequest(client_fd, &(ack)) <= 0){
            printf("\t\tErrore invio utenti online\n");
            free(users_online);
            return -1;
        }
        free(users_online);
        printf("\t\tUtenti online inviati correttamente\n");
        return 0;
    }
    // Operazione fallita
    MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
    fprintf(stdout, "\t\tOP_FAIL\n");
    setSendAck(ack.hdr, OP_FAIL, client_fd);
    free(users_online);
    return -1;
}

/**
 * @function unregister_op
 * @brief Gestisce la richiesta di deregistrazione di un nickname 
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int unregister_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    message_t ack; 
    memset(&ack, 0, sizeof(message_t));
    fprintf(stdout, "\t\tUNREGISTER_OP: %s\n", sender);
    
    // Deregistro il sender
    int r = unregister_user(users_db, sender);        
    if (r == 0){ //Deregistrazione completata
        MUTEX_BLOCK(mtx_stats, {chattyStats.nusers--;});
        MUTEX_BLOCK(mtx_stats, {chattyStats.nonline--;});     
        printf("\t\tNickname eliminato\n");
        // Invio ack 
        if(setSendAck(ack.hdr, OP_OK, client_fd) == -1) return -1;
        return 0;
    }
    
    MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
    printf("\t\tOP_FAIL (Deregistrazione fallita)\n");
    setSendAck(ack.hdr, OP_FAIL, client_fd);
    return -1;
}

/**
 * @function disconnect_op
 * @brief Gestisce la richiesta di disconnessione del client  
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int disconnect_op(message_t msg_receved, int client_fd){
    char *sender = msg_receved.hdr.sender;
    message_t ack;
    memset(&ack, 0, sizeof(message_t));
    fprintf(stdout, "\t\tDISCONNECT_OP: %s\n", sender);
    
    int d = disconnect_user(users_db, sender);
    if(d == 0){ // Disconessione riuscita
        MUTEX_BLOCK(mtx_stats, {chattyStats.nonline--;});
        if(setSendAck(ack.hdr, OP_OK, client_fd) == -1) return -1;
        return 0;
    }
    MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});

    if(d == -1){
        fprintf(stderr, "\t\tOP_FAIL\n");
        setSendAck(ack.hdr, OP_FAIL, client_fd);
        return -1;
    }
    if(d == -2) fprintf(stderr, "\t\tUtente non registrato\n");
    if(d == -3) fprintf(stderr, "\t\tUtente gia disconnesso\n"); 
    setSendAck(ack.hdr, OP_NICK_UNKNOWN, client_fd);
    return -1;
}

/**
 * @function handler
 * @brief Gestisce le richieste dei client 
 *
 * @param msg_receved       messaggio ricevuto dal client
 * @param client_fd         descrittore della connessione
 *
 * @return 0 successo, -1 fallimento
 */
int handler(message_t msg_receved, int client_fd){
    op_t op = msg_receved.hdr.op;
    message_t ack;                       // Messaggio di acknowledge
    memset(&ack, 0, sizeof(message_t));

    switch (op){
        
        // Richiesta di registrazione di un nickname 
        case REGISTER_OP:{
            return register_op(msg_receved, client_fd);
        }

        // Richiesta di connessione di un client 
        case CONNECT_OP:{
            return connect_op(msg_receved, client_fd); 
        }

        // Richiesta di invio di un messaggio testuale ad un nickname
        case POSTTXT_OP:{
            return posttxt_op(msg_receved, client_fd);    
        }

        // Invio di un messaggio testuale a tutti gli utenti registrati 
        case POSTTXTALL_OP:{
            return posttxtall_op(msg_receved, client_fd);     
        }

        // Richiesta di invio di un file ad un nickname
        case POSTFILE_OP:{
            return postfile_op(msg_receved, client_fd);
        }

        // Richiesta di recupero di un file inviato da un altro nickname  
        case GETFILE_OP:{
            return getfile_op(msg_receved, client_fd);
        }

        // Richiesta di recupero degli ultimi messaggi inviati al client      
        case GETPREVMSGS_OP:{
            return getprevmsgs_op(msg_receved, client_fd);
        }

        // Richiesta della lista di tutti i nickname connessi  
        case USRLIST_OP:{
            return usrlist_op(msg_receved, client_fd);
        }


        // Richiesta di deregistrazione di un nickname   
        case UNREGISTER_OP:{
            return unregister_op(msg_receved, client_fd);
        }

        // Richiesta di disconnessione del client  
        case DISCONNECT_OP:{
            return disconnect_op(msg_receved, client_fd);
        }

        default:{   
            fprintf(stderr,"Errore handler, operazione non trovata : %s\n", msg_receved.hdr.sender);
            MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
            setSendAck(ack.hdr, OP_FAIL, client_fd);
            return -1;
        }
    }
}

/**
 * @function thread_worker
 * @brief Funzione eseguita dai thread presenti nel pool
 *
 * @param arg       thread id 
 *
 * @return null
 */
void *thread_worker(void *arg){
    message_t msg_c;
    int thid = (intptr_t) arg;
    fprintf(stdout, "\tWorker %d start\n", thid);

    while (!stop){
        int *tmp, connfd;
        memset(&msg_c, 0, sizeof(message_t));
        // Pop file descriptor dalla coda 
        tmp = (void *) pop(q);
        connfd = *tmp;
        // Controllo se è stata richiesta la terminazione del server
        if(connfd == -2){
            push(q, tmp);
            return NULL;
        }
        free(tmp);
        
        // Leggo il messaggio del client 
        if(readMsg(connfd, &msg_c) > 0){ 
            fprintf(stdout, "\tWorker %d (Messaggio del client [fd:%d] letto correttamente)\n", thid, connfd);
            fprintf(stdout, "\tWorker %d (Operazione richiesta da %s)\n", thid, msg_c.hdr.sender);
            
            // Gestione richiesta del client 
            if (handler(msg_c, connfd) == 0){
                fprintf(stdout, "\tWorker %d (handler concluso correttamente)\n", thid);
                pthread_mutex_lock(&mtx_set);
                FD_SET(connfd, &set);     // Inserisco l'fd nel set della select
                pthread_mutex_unlock(&mtx_set);
            }else{
                // Gesione richiesta fallita
                fprintf(stderr, "\tWorker %d (handler fallito)\n", thid);
                if(disconnect_user_fd(users_db, connfd) == 0){        // Se connesso lo disconnetto altrimenti non faccio nulla 
                    MUTEX_BLOCK(mtx_stats, {chattyStats.nonline--;});
                }
            }
        }else{
            fprintf(stdout, "\tWorker %d (Nessuna richiesta dal client... disconnetto)\n", thid);
            if(disconnect_user_fd(users_db, connfd) == 0){            // Se connesso lo disconnetto altrimenti non faccio nulla 
                MUTEX_BLOCK(mtx_stats,{chattyStats.nonline--;});
            }
        }
        if(msg_c.data.buf != NULL)
            free(msg_c.data.buf); 
    }
    return NULL;
}

int main(int argc, char *argv[]){

    // Controllo parametri in ingresso
    if (argc != 3 || strncmp(argv[1], "-f", 2) != 0){
        usage(argv[0]);
        return -1;
    }

    int i, notused;

    fprintf(stdout, "Lettura file di configurazione...\n");

    // Parsing file di configurazione
    CHECK_MENO1(notused, parsing(argv[2], &configuration), "Parsing" );

    // Stampa parametri di configurazione server
    fprintf(stdout, "Parsing concluso\n\n");
    fprintf(stdout, "Paramentri di configurazione server:\n");
    fprintf(stdout, "************************************\n");
    fprintf(stdout, "UnixPath: %s\n", configuration.UnixPath);
    fprintf(stdout, "DirName: %s\n", configuration.DirName);
    fprintf(stdout, "StatFileName: %s\n", configuration.StatFileName);
    fprintf(stdout, "MaxConnections: %d\n", configuration.MaxConnections);
    fprintf(stdout, "ThreadsInPool: %d\n", configuration.ThreadsInPool);
    fprintf(stdout, "MaxMsgSize: %d\n", configuration.MaxMsgSize);
    fprintf(stdout, "MaxFileSize: %d\n", configuration.MaxFileSize);
    fprintf(stdout, "MaxHistMsgs: %d\n", configuration.MaxHistMsgs);
    fprintf(stdout, "************************************\n");
    fprintf(stdout, "\n");

    unlink(configuration.UnixPath);   

    // Gestione segnali 
    sigemptyset(&sigset);      // Resetto tutti i bits
    sigaddset(&sigset,SIGINT);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGQUIT);
    sigaddset(&sigset,SIGUSR1);
    sigaddset(&sigset,SIGUSR2);
    sigaddset(&sigset,SIGPIPE);

    // Maschermo i segnali impostati nel set
    pthread_sigmask(SIG_SETMASK, &sigset, NULL);

    // Creo il thread per la cattura dei segnali 
    if(pthread_create(&sigTread, NULL, sigHandler, (void *) NULL) != 0){
        fprintf(stderr,"[Main] Creazione sigHandler fallita\n");
        exit(EXIT_FAILURE); 
    }

    //Creo il socket
    int fd_socket;
    SYSCALL(fd_socket, socket(AF_UNIX, SOCK_STREAM, 0), "socket");

    // Inizializzazione strutture dati  
    users_db = users_db_create(NBUCKETS, configuration.MaxConnections, configuration.MaxHistMsgs); 
    if(users_db == NULL){
        fprintf(stderr,"[Main] Iniziallizzazione strutture dati fallita\n");
        exit(EXIT_FAILURE);
    }

    // Inizializzazione coda  
    q = initQueue();
    if(q == NULL){
        fprintf(stderr,"[Main] Iniziallizzazione coda fallita\n");
        exit(EXIT_FAILURE);
    } 

    // Imposta un flag sulla libreria Connections.c per abilitare la mutua esclusione
    initConnection();

    struct sockaddr_un sa;
    strncpy(sa.sun_path, configuration.UnixPath, strlen(configuration.UnixPath) + 1);
    sa.sun_family = AF_UNIX;
    SYSCALL(notused, bind(fd_socket, (struct sockaddr *)&sa, sizeof(sa)), "bind");  
    SYSCALL(notused, listen(fd_socket, configuration.MaxConnections), "listen");     
    fprintf(stdout, "[Main] Server start\n");

    // Set dei fd da ascoltare
    int connfd, fd, fd_max = 0;
    fd_set tmpset;
    FD_ZERO(&set);
    FD_ZERO(&tmpset);
    FD_SET(fd_socket, &set);

    fd_max = fd_socket; 

    // Creazione ThreadPool 
    threadPool = (pthread_t *) Malloc(configuration.ThreadsInPool * sizeof(pthread_t));

    for(i = 0; i < configuration.ThreadsInPool; i++){
        if(pthread_create(&threadPool[i], NULL, thread_worker, (void *) (intptr_t) i) != 0){
            fprintf(stderr,"Creazione worker %d fallita\n", i); 
        }
        fprintf(stdout, "Worker %d creato\n", i);
    }

    int nonline;

    // Loop del server
    while (!stop){ 
        pthread_mutex_lock(&mtx_set);
        tmpset = set;
        pthread_mutex_unlock(&mtx_set);

        // Timiout select 1000 usec
        struct timeval tv = {0, 1000}; 

        int res = select(fd_max + 1, &tmpset, NULL, NULL, &tv);
        if(res < 0) continue;
        for (fd = 0; fd <= fd_max; fd++){
            if (FD_ISSET(fd, &tmpset)){
                if (fd == fd_socket){ 
                    // Richiesta di connesione 
                    SYSCALL(connfd, accept(fd_socket, (struct sockaddr *)NULL, NULL), "accept");
                      
                    MUTEX_BLOCK(mtx_stats, {nonline = chattyStats.nonline;});
                
                    // Controllo limite connessini   
                    if(nonline >= configuration.MaxConnections){
                        fprintf(stdout, "[Main] Connessioni massime raggiunte\n");
                        message_t ack;
                        MUTEX_BLOCK(mtx_stats, {chattyStats.nerrors++;});
                        setSendAck(ack.hdr, OP_FAIL, connfd);

                    }
                    else{
                        pthread_mutex_lock(&mtx_set);
                        FD_SET(connfd, &set);          // Aggiungo connfd all' insieme set
                        pthread_mutex_unlock(&mtx_set);
                        if (connfd > fd_max) fd_max = connfd; 
                    }
                }
                else{ // Richiesta da un client connesso
                    fprintf(stdout, "[Main] Richiesta da client [fd:%d]\n", fd);

                    int *data = Calloc(1,sizeof(int));
                    *data = fd; 
            
                    pthread_mutex_lock(&mtx_set);
                    FD_CLR(fd, &set);               // Non gestisto piu il client
                    pthread_mutex_unlock(&mtx_set);
                    
                    // Inserimento fd nella coda
                    push(q, data);
                }
            }
        }
    }

    /************************************ Gestione chiusura server ************************************/
    //Inserisco nella coda l'EOS
    int *eos = Calloc(1, sizeof(int));
    *eos = -2;
    push(q, eos);

    pthread_join(sigTread, NULL);
    fprintf(stdout, "[Main] Join thread sigwait\n");

    // Aspetto i thread che terminino
    for (i = 0; i < configuration.ThreadsInPool; i++){
        pthread_join(threadPool[i], NULL);
        fprintf(stdout, "[Main] Join thread %d\n", i);
    }

    //Libero memoria allocata precedentemente
    fprintf(stdout, "[Main] Pulizia memoria...\n");
    deleteQueue(q);
    users_db_destroy(users_db);
    close(fd_socket);
    free(users_db);
    free(threadPool);
    destroyConnection();
    free(eos);
    fprintf(stdout, "Server chiuso.\n");
    return 0;
}

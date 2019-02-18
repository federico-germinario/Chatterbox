/**
 * @file  user.h
 * @brief Libreria contenente la struttura dati del server e le relative funzioni
 *        per la sua gestione
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */
#include <stdio.h>
#include "icl_hash.h"
#include "config.h"
#include "history.h"

/**
 *  @struct user
 *  @brief Struttura utente registrato
 *
 *  @var name       nickname 
 *  @var fd         file descriptor del nickname
 *  @var history    puntatore alla history del nickname     
 */
typedef struct {
    char name[MAX_NAME_LENGTH + 1];
    int fd;
    history_t *history; 
}user_t;

/**
 *  @struct user_online
 *  @brief Struttura utente online
 *
 *  @var name       nickname 
 *  @var fd         file descriptor del nickname   
 */
typedef struct {
    char name[MAX_NAME_LENGTH + 1];
    int fd;
}user_online_t;

/**
 *  @struct users_db
 *  @brief Struttura dati utilizzata dal server
 *
 *  @var db                 puntatore a una tabella hash contenente gli utenti registrati 
 *  @var users_online       puntatore a un array contenente gli utenti online
 *  @var n_users_online     numero utenti online
 *  @var history_size       dimensione massima history per ogni utente
 *  @var max_connections    connessioni massime contemporanee accettate dal server
 */
typedef struct {
    icl_hash_t *db;                  
    user_online_t *users_online;     
    int n_users_online;             
    int history_size;               
    int max_connections;            
}users_db_t;

/**
 * @function add_user_online
 * @brief Aggiunge un utente all' array degli utenti connessi in mutua escusione
 *
 * @param users_db          puntatore alla struttura dati del server
 * @param name              nome utente da inserire nell' array
 *
 * @returns 0 successo, -1 fallimento
 */
int add_user_online(users_db_t *users_db, char * name, int fd);

/**
 * @function delete_user_online
 * @brief Elimina un utente dall' array degli utenti connessi in mutua escusione
 *
 * @param users_db          puntatore alla struttura dati del server
 * @param name              nome utente da eliminare dall' array
 *
 * @returns 0 successo, -1 fallimento
 */
int delete_user_online(users_db_t *users_db, char * name);

/**
 * @function users_db_create
 * @brief Inizializza le strutture dati del server
 *
 * @param nbuckets          dimensione tabella hash utenti registrati
 * @param maxconnections    numero massimo di connessioni gestite dal server
 * @param history_size      numero massimo di messaggi che il server 'ricorda' per ogni client
 *
 * @returns puntatore al nuovo users_db 
 */
users_db_t * users_db_create(int nbuckets, int max_connections, int history_size);

/**
 * @function users_db_destroy
 * @brief Distrugge le strutture dati del server
 *
 * @param users_db      puntatore alla struttura dati da distruggere 
 *
 * @returns 0 successo, -1 fallimento 
 */
int users_db_destroy(users_db_t *users_db);

/**
 * @function register_user
 * @brief Registrazione utente 
 *
 * @param users_db      puntatore alla struttura dati del server 
 * @param name          nome utente da registrare
 *
 * @returns 0 successo, -1 utente gia' registrato, -2 fallimento
 */
int register_user(users_db_t *users_db, char *name);

/**
 * @function unregister_user
 * @brief Deregistrazione utente
 *
 * @param users_db       puntatore alla struttura dati del server 
 * @param name           nome utente da deregistrare
 *
 * @returns 0 successo, -1 utente non registrato, -2 fallimento
 */
int unregister_user(users_db_t *users_db, char *name);

/**
 * @function get_users_online
 * @brief Lista utenti online
 *
 * @param users_db       puntatore alla struttura dati del server 
 * @param list_online    puntatore alla lista dove andare a scrivere
 *                       i nomi degli utenti online  
 *
 * @returns >= 0 numero utenti online, -1 fallimento
 */
int get_users_online(users_db_t *users_db, char **list_online);

/**
 * @function connect_user
 * @brief Connessione utente
 *
 * @param users_db      puntatore alla struttura dati del server  
 * @param name          nome utente da connettere
 * param fd             file descriptor dell'utente
 *
 * @returns 0 successo, -1 fallimento, -2 utente non registrato, -3 utente gia connesso
 */
int connect_user(users_db_t *users_db, char* name, int fd);

/**
 * @function disconnect_user
 * @brief Disconnesione utente utilizzando il suo nome
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param name          nome utente da disconnettere
 *
 * @returns 0 successo, -1 fallimento, -2 utente non registrato, -3 utente gia disconnesso.
 */
int disconnect_user(users_db_t *users_db, char *name);

/**
 * @function disconnect_user_fd
 * @brief Disconnesione utente utilizzando il suo file descriptor
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param fd            file descriptor dell' utente da disconnettere
 *
 * @returns 0 successo, -1 fallimento, -2 utente non registrato, -3 utente gia disconnesso.
 */
int disconnect_user_fd(users_db_t *users_db, int fd);

/**
 * @function get_user
 * @brief Restituisce la struttura dell' utente name
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param name          nome utente di cui si vuole recuperare la struttura
 *
 * @returns puntatore alla struttura dati dell' utente
 */
user_t * get_user(users_db_t *users_db, char* name);      

/**
 * @function history_sender
 * @brief Restitisce la history di name
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param name          nome utente di cui si vuole recuperare la history
 *
 * @returns puntatore alla history
 */
history_t * history_sender(users_db_t *users_db, char *name);

/**
 * @file  user.c
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include "icl_hash.h"
#include "user.h"
#include "config.h"
#include "util.h"

#define DEFAULT_NBUCKETS_HASH 1024

// Mutex utilizzata per le operazioni in m.e sull' array degli utenti connessi 
static pthread_mutex_t online_mtx = PTHREAD_MUTEX_INITIALIZER;

/**
 * @function add_user_online
 * @brief Aggiunge un utente all' array degli utenti connessi in mutua esclusione
 *
 * @param users_db          puntatore alla struttura dati del server
 * @param name              nome utente da inserire nell' array
 *
 * @returns 0 successo, -1 fallimento
 */
int add_user_online(users_db_t *users_db, char * name, int fd){
    if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return -1;
    }
    int i = 0;
    int stop = 0;
    pthread_mutex_lock(&online_mtx);
    // Itero sull' array 
    while(!stop && i < users_db->max_connections){
        if(strcmp(users_db->users_online[i].name, "") == 0 ){
            strncpy(users_db->users_online[i].name, name, MAX_NAME_LENGTH + 1);
            users_db->users_online[i].fd = fd;
            stop = 1;
        }
        i++;
    }
    pthread_mutex_unlock(&online_mtx);
    if(!stop) return -1;
    users_db->n_users_online ++;
    return 0;
}

/**
 * @function delete_user_online
 * @brief Elimina un utente dall' array degli utenti connessi in mutua esclusione
 *
 * @param users_db          puntatore alla struttura dati del server
 * @param name              nome utente da eliminare dall' array
 *
 * @returns 0 successo, -1 fallimento
 */
int delete_user_online(users_db_t *users_db, char * name){
    if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return -1;
    }
    int i = 0;
    int stop = 0;
    pthread_mutex_lock(&online_mtx);
    // Itero sull' array fino a quando non trovo name
    while(!stop && i < users_db->max_connections){
        if(strncmp(users_db->users_online[i].name, name, MAX_NAME_LENGTH + 1) == 0 ){
            strcpy(users_db->users_online[i].name, "");
            users_db->users_online[i].fd = -1;
            stop = 1;
        }
        i++;
    }
    if(!stop){
        pthread_mutex_unlock(&online_mtx);
        return -1;
    }
    users_db->n_users_online--;
    pthread_mutex_unlock(&online_mtx);
    return 0;
}

/**
 * @function free_data 
 * @brief Dealloca risorse del campo data della hash table
 *
 * @param user      puntatore all' utente da deallocare 
 */
void free_data(void *user){
    user_t *tmp = (user_t *) user;
    destroyHistory(tmp->history);
    free(tmp);
    tmp = NULL;
}

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
users_db_t * users_db_create(int nbuckets, int max_connections, int history_size){
    if(max_connections <= 0){
        errno = EINVAL;
        return NULL;
    }

    // Controllo numero buckets della tabella hash 
    if(nbuckets <= 0) nbuckets = DEFAULT_NBUCKETS_HASH;

    users_db_t * users_db = (users_db_t *) Calloc(1,sizeof(users_db_t));
    users_db->max_connections = max_connections;
    // Inizializzo tabella hash 
    users_db->db = icl_hash_create(nbuckets, NULL, NULL);
    if(users_db->db == NULL) return NULL;
    users_db->users_online = (user_online_t *) Calloc(max_connections, sizeof(user_online_t));
    
    // Inizializzo l'array degli utenti online 
    for(int i = 0; i < max_connections; i++){
        strncpy(users_db->users_online[i].name, "", 1);
        users_db->users_online[i].fd = -1;
    }
    
    users_db->history_size = history_size;
    return users_db;
}

/**
 * @function users_db_destroy
 * @brief Distrugge le strutture dati del server
 *
 * @param users_db      puntatore alla struttura dati da distruggere 
 *
 * @returns 0 successo, -1 fallimento 
 */
int users_db_destroy(users_db_t *users_db){
    if(users_db == NULL){
        errno = EINVAL;
        return -1;
    }
    if(!icl_hash_destroy(users_db->db, NULL, free_data)){ 
        free(users_db->users_online);
        return 0;
    }
    return -1;
}

/**
 * @function register_user
 * @brief Registrazione utente 
 *
 * @param users_db      puntatore alla struttura dati del server 
 * @param name          nome utente da registrare
 *
 * @returns 0 successo, -1 utente gia' registrato, -2 fallimento
 */
int register_user(users_db_t *users_db, char * name){ 
    if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return -2;
    }

    // Prendo la mutua esclusione sulla sezione della tabella hash che contiene name come chiave 
    lock_hash_section(users_db->db, name);
    user_t *user = (user_t *) Malloc(sizeof(user_t));          // Creo un nuovo utente
    strncpy(user->name, name, MAX_NAME_LENGTH + 1);   
    user->history = createHistory(users_db->history_size);     // Creo una nuova history per l'utente
    user->fd = -1;

    // Inserisco il nuovo utente nella tabella hash 
    int ret = icl_hash_insert(users_db->db, user->name, (void *)user);
    if(ret == -1 || ret == -2){ // Utente gia registrato o errore generico 
        free_data(user);
        unlock_hash_section(users_db->db, name);
        return ret;
    }
    unlock_hash_section(users_db->db, name);
    return 0;
}

/**
 * @function unregister_user
 * @brief Deregistrazione utente
 *
 * @param users_db       puntatore alla struttura dati del server 
 * @param name           nome utente da deregistrare
 *
 * @returns 0 successo, -1 utente non registrato, -2 fallimento
 */
int unregister_user(users_db_t *users_db, char *name){
    if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return -2;
    }

    // Prendo la mutua esclusione sulla sezione della tabella hash che contiene name come chiave 
    lock_hash_section(users_db->db, name);

    // Controllo che l'utente sia registrato
    user_t *user = icl_hash_find(users_db->db, name);
    if(user == NULL){
        unlock_hash_section(users_db->db, name);
        return -1;
    }
   
    // Elimino l'utente dall' array degli utenti connessi 
    delete_user_online(users_db, name);

    // Elimino l'utente dalla tabella hash deallocando la memoria allocata
    // precedentemente 
    int ret = icl_hash_delete(users_db->db, name, NULL, free_data);   
    unlock_hash_section(users_db->db, name);
    return ret;
}

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
int get_users_online(users_db_t *users_db, char **list_online){
     if(users_db == NULL){
        errno = EINVAL;
        return -1;
    }

    // Prendo la mutua esclusione sull' array degli utenti connessi
    pthread_mutex_lock(&online_mtx);
    int ret = users_db->n_users_online;
    *list_online = (char *)Calloc(ret * (MAX_NAME_LENGTH + 1), sizeof(char));
    char * tmp = *list_online;

    // Itero sull'array 
    for(int i = 0; i < users_db->max_connections; i++){
        if(strncmp(users_db->users_online[i].name, "", 1) != 0 ){
            strncpy(tmp, users_db->users_online[i].name, MAX_NAME_LENGTH + 1);
            tmp += MAX_NAME_LENGTH + 1;
        }
    }
    pthread_mutex_unlock(&online_mtx);

    return ret;
}

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
int connect_user(users_db_t *users_db, char* name, int fd){
    if(users_db == NULL || name == NULL || fd < 0){
        errno = EINVAL;
        return -1;
    }
    
    // Prendo la mutua esclusione sulla sezione della tabella hash che contiene name come chiave 
    lock_hash_section(users_db->db, name);
    
    user_t *user = icl_hash_find(users_db->db, name);
    if(user == NULL){        // Utente non registrato
        unlock_hash_section(users_db->db, name);
        return -2;
    }else if(user->fd != -1){ // Utente gia connesso
        unlock_hash_section(users_db->db, name);
        return -3;
    }
    // Utente registrato ma non connesso
     
    // Aggiorno informazioni sull' utente
    user->fd = fd;
    int ret = add_user_online(users_db, name, fd);
    unlock_hash_section(users_db->db, name);
    return ret;
}

/**
 * @function disconnect_user
 * @brief Disconnesione utente utilizzando il suo nome
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param name          nome utente da disconnettere
 *
 * @returns 0 successo, -1 fallimento, -2 utente non registrato, -3 utente gia disconnesso.
 */
int disconnect_user(users_db_t *users_db, char *name){
     if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return -1;
    }

    // Prendo la mutua esclusione sulla sezione della tabella hash che contiene name come chiave 
    lock_hash_section(users_db->db, name);
    
    //Controllo che l'utente sia registrato
    user_t *user = icl_hash_find(users_db->db, name);
    if (user == NULL){          // Utente non registrato
        unlock_hash_section(users_db->db, name);  
        return -2;
    }else if(user->fd == -1){   // Utente gia disconnesso
        unlock_hash_section(users_db->db, name);
        return -3;
    }
    //Utente registrato e online 
    
    user->fd = -1;
    int ret = delete_user_online(users_db, name);
    unlock_hash_section(users_db->db, name);
    return ret;
}

/**
 * @function disconnect_user_fd
 * @brief Disconnesione utente utilizzando il suo file descriptor
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param fd            file descriptor dell' utente da disconnettere
 *
 * @returns 0 successo, -1 fallimento, -2 utente non registrato, -3 utente gia disconnesso.
 */
int disconnect_user_fd(users_db_t *users_db, int fd){
    if(users_db == NULL || fd < 0){
        errno = EINVAL;
        return -1;
    }

    int i = 0;
    int stop = 0;

    char *name;  
    
    // Prendo la mutua esclusione sull' array degli utenti connessi
    pthread_mutex_lock(&online_mtx);

    // Cerco nell'array il file descriptor, ricavo il suo nome utente e lo elimino  
    while(!stop && i < users_db->max_connections){
        if(users_db->users_online[i].fd == fd ){
            name = Calloc((strlen(users_db->users_online[i].name) + 1), sizeof(char));
            strncpy(name, users_db->users_online[i].name,(strlen(users_db->users_online[i].name) + 1));
            strncpy(users_db->users_online[i].name, "", 1);
            users_db->users_online[i].fd = -1;
            stop = 1;
        }
        i++;
    }
    pthread_mutex_unlock(&online_mtx);

    // Controllo se ho trovato in nome utente relativo a fd 
    if(stop != 1){ 
        return -1;
    }

    // Prendo la mutua esclusione sulla sezione della tabella hash che contiene name come chiave 
    lock_hash_section(users_db->db, name);

    //Controllo che l'utente sia registrato
    user_t *user = icl_hash_find(users_db->db, name);
    if (user == NULL){          // Utente non registrato
        unlock_hash_section(users_db->db, name);
        free(name);  
        return -2;
    }else if(user->fd == -1){   // Utente gia disconnesso
        unlock_hash_section(users_db->db, name);
        free(name);
        return -3;
    }
   
    user->fd = -1;
    users_db->n_users_online --;  // Aggiorno numero utenti online
    unlock_hash_section(users_db->db, name);
    free(name);
    return 0;
}

/**
 * @function get_user
 * @brief Restituisce la struttura dell' utente name
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param name          nome utente di cui si vuole recuperare la struttura
 *
 * @returns puntatore alla struttura dati dell' utente
 */
user_t * get_user(users_db_t *users_db, char* name){
    if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return NULL;
    }

    lock_hash_section(users_db->db, name);
    user_t *user = icl_hash_find(users_db->db, name);
    unlock_hash_section(users_db->db, name);

    return user;
}     

/**
 * @function history_sender
 * @brief Restitisce la history di name
 *
 * @param users_db      puntatore alla struttura dati del server
 * @param name          nome utente di cui si vuole recuperare la history
 *
 * @returns puntatore alla history
 */
history_t *history_sender(users_db_t *users_db, char *name){
    if(users_db == NULL || name == NULL){
        errno = EINVAL;
        return NULL;
    }
    
    // Prendo la mutua esclusione sulla sezione della tabella hash che contiene name come chiave 
    lock_hash_section(users_db->db, name);
    user_t *user = icl_hash_find(users_db->db, (void *) name);
    if(user == NULL){
        unlock_hash_section(users_db->db, name);
        return NULL;
    }
    history_t * ret = user->history;
    unlock_hash_section(users_db->db, name);
    return ret;
}


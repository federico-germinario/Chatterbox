/**
 * @file  history.c
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "history.h"
#include "util.h"

/**
 * @function createHistory
 * @brief Crea una nuova history
 *
 * @param MaxHistMsgs      dimensione massima history 
 *
 * @return puntatore alla nuova history
 */
history_t* createHistory(int MaxHistMsg){
    history_t *history = (history_t *) Calloc(1, sizeof(history_t));
    history->msgs   = Calloc(MaxHistMsg, sizeof(message_t*));
    int i;
    for(i = 0; i < MaxHistMsg; i++ ){
        history->msgs[i] = NULL;
    }
    history->head   = 0; 
    history->dim    = 0;
    history->dimMax = MaxHistMsg;
    if(pthread_mutex_init(&(history->mtx), NULL) != 0){
        return NULL;
    }
    return history;
}

/**
 * @function destroyHistory
 * @brief Dealloca le strutture dati della history
 *
 * @param history      puntatore history 
 *
 * @return 0 successo, -1 fallimento
 */
int destroyHistory(history_t *history){
    if(history == NULL){
        errno = EINVAL;
        return -1;
    }
    int i;
    for( i=0; i<history->dimMax; i++ ){
        if(history->msgs[i] != NULL) 
            freeMessage(history->msgs[i]);
    }
    free(history->msgs);
    if( pthread_mutex_destroy(&(history->mtx)) != 0) return -1;
    free(history);
    return 0;
}

/**
 * @function insertMsg
 * @brief Inserisce un messaggio nella history in mutua escusione
 *
 * @param history      puntatore alla history dove inserire il messaggio 
 * @param msg          puntatore al messaggio da inserire
 * 
 * @return 0 successo , -1 fallimento
 */
int insertMsg(history_t *history, message_t *msg){
    if(history == NULL){
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&(history->mtx));

    // La coda ha raggiunto la dimensione massima
    if(history->dim == history->dimMax){
        freeMessage( history->msgs[history->head] );
        history->msgs[history->head] = msg;
        history->head = (history->head + 1) % history->dimMax;
    }else{
        history->msgs[history->dim] = msg;
        history->dim++;
    }

    pthread_mutex_unlock(&(history->mtx));
    return 0;
}

/**
 * @function outMsg
 * @brief Estrae tutti i messaggi presenti
 *
 * @param history      puntatore history 
 * @param msg_list     puntatore dove memorizzare i messagi estratti 
 * 
 * @return numero messaggi da leggere, -1 fallimento
 */
int outMsg(history_t *history, message_t ***msg_list){
    if(history == NULL || msg_list == NULL){
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutex_lock(&(history->mtx));
    int dim = history->dim;
    if(dim == 0){
        pthread_mutex_unlock(&(history->mtx));
        return 0;
    } 

    *msg_list = Calloc(dim, sizeof(message_t));
    for(int i = 0; i < dim; i++){
        (*msg_list)[i] = history->msgs[(i + history->head) % history->dimMax];  // Mi ricordo del primo messaggio   
        history->msgs[(i + history->head) % history->dimMax] = NULL;
    }
    history->head   = 0; 
    history->dim    = 0;
    pthread_mutex_unlock(&(history->mtx));
    
    return dim;
}
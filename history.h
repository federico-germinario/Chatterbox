/**
 * @file  history.h
 * @brief History circolare con concorrenza
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include "message.h"
#include <stdio.h>
#include <pthread.h>

/**
 *  @struct HISTORY
 *  @brief Struttura history dei messaggi
 *
 *  @var msgs       Messaggi memorizzati
 *  @var head       Testa dell'array
 *  @var dim        Dimensione corrente array
 *  @var dimMax     Dimensione massima array
 *  @var mtx        Mutex per mutua esclusione
 */
typedef struct {
    message_t **msgs;    
    int head;            
    int dim;             
    int dimMax;          
    pthread_mutex_t mtx; 
} history_t;

/**
 * @function createHistory
 * @brief Crea una nuova history
 *
 * @param MaxHistMsgs      dimensione massima history 
 *
 * @return puntatore alla nuova history
 */
history_t *createHistory(int MaxHistMsgs);

/**
 * @function destroyHistory
 * @brief Dealloca le strutture della history
 *
 * @param history      puntatore history 
 *
 * @return 0 successo, -1 fallimento
 */
int destroyHistory(history_t *history);

/**
 * @function insertMsg
 * @brief Inserisce un messaggio nella history in mutua escusione
 *
 * @param history      puntatore alla history dove inserire il messaggio 
 * @param msg          puntatore al messaggio da inserire
 * 
 * @return 0 successo , -1 fallimento
 */
int insertMsg(history_t *history, message_t *msg);

/**
 * @function outMsg
 * @brief Estrae tutti i messaggi presenti
 *
 * @param history      puntatore history 
 * @param msg_list     puntatore dove memorizzare i messagi estratti 
 * 
 * @return numero messaggi da leggere, -1 fallimento
 */
int outMsg(history_t *history, message_t ***msg_list);



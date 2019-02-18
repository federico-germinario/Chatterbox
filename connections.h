/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include <message.h>

/**
 * @file  connection.h
 * @brief Contiene le funzioni che implementano il protocollo 
 *        tra i clients ed il server
 * 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */


/**
 * @function initConnection
 * @brief Inizializza le mutex e imposta un flag
 * 
 * @return -1 errore, 0 successo
 */
int initConnection();

/**
 * @function destroyConnection
 * @brief Distrugge le risorse allocate precedentemente 
 * 
 */
void destroyConnection();

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server 
 *
 * @param path Path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs);

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readHeader(long connfd, message_hdr_t *hdr);

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readData(long fd, message_data_t *data);

/**
 * @function readMsg
 * @brief Legge l'intero messaggio in mutua esclusione se il flag è settato
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readMsg(long fd, message_t *msg);

/**
 * @function sendHeader
 * @brief Invia l'header del messaggio 
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int sendHeader(long fd, message_hdr_t *hdr);

/**
 * @function sendHeader
 * @brief Invia l'header del messaggio in mutua esclusione se il flag è settato
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int sendAck(long fd, message_hdr_t *hdr);

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg);

/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server in mutua esclusione se il flag è settato
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg);

/**
 * @function setSendAck
 * @brief Scrive l'header del messaggio e lo invia al client 
 *
 * @param ack           puntatore all'header
 * @param op            tipo di operazione da inviare
 * @param client_fd     descrittore della connessione 
 *
 * @return 0 successo, -1 fallimento
 */
int setSendAck(message_hdr_t ack, op_t op, int client_fd);

#endif /* CONNECTIONS_H_ */
/**
 * @file  connection.c
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "util.h"
#include "connections.h"

#define NSECTIONS 4

static pthread_mutex_t *mtx_conn; 
int flag = 0;                       // Flag utilizzato per abilitare la mutua esclusione 

/**
 * @function readn
 * @brief Legge esattamente size byte 
 *
 * @param fd      descrittore su cui leggere 
 * @param buf     puntatore al buffer dove andare a scrivere i byte letti
 * @param size    byte da leggere
 *
 * @return numero byte letti 
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	    if ((r=read((int)fd ,bufptr,left)) == -1) {
	        if (errno == EINTR) continue;
	        return -1;
	    }
	    if (r == 0) return 0;   
        left    -= r;
	    bufptr  += r;
    }
    return size;
}

/**
 * @function writen
 * @brief Scrive esattamente size byte 
 *
 * @param fd      descrittore su cui scrivere 
 * @param buf     puntatore al buffer dove andare a leggere i byte da scivere
 * @param size    byte da scrivere
 *
 * @return numero byte scritti 
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	    if ((r=write((int)fd ,bufptr,left)) == -1) {
	        if (errno == EINTR) continue;
	        return -1;
	    }
	    if (r == 0) return 0;  
        left    -= r;
	    bufptr  += r;
    }
    return 1;
}

/**
 * @function initConnection
 * @brief Inizializza le mutex e imposta un flag
 * 
 * @return -1 errore, 0 successo
 */
int initConnection(){
    flag = 1;      // Abilito la mutua esclusione 
    mtx_conn = calloc (NSECTIONS, sizeof(pthread_mutex_t)); 
    if(mtx_conn == NULL){
        return -1;
    }
    for(int i=0; i < NSECTIONS; i++){
        pthread_mutex_init( &mtx_conn[i], NULL );
    }
    return 0;
}

/**
 * @function destroyConnection
 * @brief Distrugge le risorse allocate precedentemente 
 * 
 * @return 0 successo
 */
void destroyConnection(){
    for(int i=0; i < NSECTIONS; i++){
        pthread_mutex_destroy( &mtx_conn[i]);
    }
    free(mtx_conn);
}

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
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
    struct sockaddr_un sa;
    int sockfd;
    SYSCALL(sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
    memset(&sa, '0', sizeof(sa));

    sa.sun_family = AF_UNIX;    
    strncpy(sa.sun_path, path, strlen(path)+1);

    while(ntimes-- && connect(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1){ //Tento 10 connessioni
        sleep(secs);
    }
    if(ntimes) return sockfd;
    return -1;
}

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
int readHeader(long connfd, message_hdr_t *hdr){
    memset(hdr, 0, sizeof(message_hdr_t));
    return readn(connfd, hdr, sizeof(message_hdr_t));
}

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
int readData(long fd, message_data_t *data){
    memset(data, 0, sizeof(message_data_t));
    
    int r = readn(fd, &(data->hdr), sizeof(message_data_hdr_t));
    if(r  <= 0) return r;
    if(data->hdr.len == 0){ // Buffer vuoto
        data->buf = NULL;
    }else{
        data->buf = calloc(data->hdr.len ,sizeof(char));
        if(data->buf == NULL){
            return -1;
        }
        if((r= readn(fd, data->buf, data->hdr.len)) <= 0){
            free(data->buf);
        }
    }
    return r;
}

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
int readMsg(long fd, message_t *msg){
    int ind = fd % NSECTIONS;        //Calcolo indice mutex
    if(flag) pthread_mutex_lock(&mtx_conn[ind]);
    memset(msg, 0, sizeof(message_t));
    int r = readHeader(fd, &(msg->hdr));
    if(r <= 0){
        if(flag) pthread_mutex_unlock(&mtx_conn[ind]);
        return r;
    } 
    r = readData(fd, &(msg->data)); 
    if(flag) pthread_mutex_unlock(&mtx_conn[ind]);
    return r;
}

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
int sendHeader(long fd, message_hdr_t *hdr){
    return writen(fd, hdr, sizeof(message_hdr_t));
}

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
int sendAck(long fd, message_hdr_t *hdr){
    int ind = fd % NSECTIONS;
    if(flag) pthread_mutex_lock(&mtx_conn[ind]);
    int r = writen(fd, hdr, sizeof(message_hdr_t));
    if(flag) pthread_mutex_unlock(&mtx_conn[ind]);
    return r;
}

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg) {
    int r1 = writen(fd, &(msg->hdr), sizeof(message_data_hdr_t));
    if(r1 <= 0) return r1;
    int r2 = writen(fd, msg->buf, msg->hdr.len);
    if(r2 <= 0) return r2;
    return r1 + r2;
}

/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server in mutua esclusione se il flag è settato
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg){
    int ind = fd % NSECTIONS;
    if(flag) pthread_mutex_lock(&mtx_conn[ind]);
    int r1 = sendHeader(fd, &(msg->hdr));
    if(r1 <= 0){
        if(flag) pthread_mutex_unlock(&mtx_conn[ind]);
        return r1;
    } 
    int r2 = sendData(fd, &(msg->data));
    if(r2 <= 0){
        if(flag) pthread_mutex_unlock(&mtx_conn[ind]);
        return r2;
    } 
    if(flag) pthread_mutex_unlock(&mtx_conn[ind]);
    return r1 + r2;
}

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
int setSendAck(message_hdr_t ack, op_t op, int client_fd){
    setHeader(&ack, op, "");
    if (sendAck(client_fd, &ack) <= 0){
        fprintf(stderr, "\t\tErrore invio ack\n");         
        return -1;
    }
    return 0;
}
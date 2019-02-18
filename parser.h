/**
 * @file  parser.h
 * @brief Parser file di configurazione server 
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include <stdio.h>
#define MAX_LINESIZE 1024

/**
* @struct serverConf
* @brief Parametri di configurazione del server
*
* @var UnixPath             Path utilizzato per la creazione del socket AF_UNIX
* @var DirName              Directory dove memorizzare i files da inviare agli utenti
* @var StatFileName         File nel quale verranno scritte le statistiche
* @var MaxConnections       Numero massimo di connessioni concorrenti gestite dal server
* @var ThreadsInPool        Numero di thread nel pool
* @var MaxMsgSize           Dimensione massima di un messaggio testuale (numero di caratteri)
* @var MaxFileSize          Dimensione massima di un file accettato dal server (kilobytes)
* @var MaxHistMsgs;         Numero massimo di messaggi che il server ’ricorda’ per ogni client
*/
struct serverConf {
    char UnixPath[MAX_LINESIZE];      
    char DirName[MAX_LINESIZE];        
    char StatFileName[MAX_LINESIZE];   
    int MaxConnections;                
    int ThreadsInPool;                 
    int MaxMsgSize;                    
    int MaxFileSize;                   
    int MaxHistMsgs;                  
};

/**
 * @function parsing
 * @brief Parsing file di configurazione
 *
 * @param path    path file da leggere
 * @param conf    puntatore alla struttura dati dove memorizzare i parametri letti
 *
 * @return 0 successo, -1 fallimento
 */
int parsing(char* path, struct serverConf *conf);
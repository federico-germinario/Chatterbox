/**
 * @file  util.h
 * @brief Macro per gestione degli errori
 * 
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#define SYSCALL(r,c,e) \
    if((r=c)==-1) { perror(e);exit(errno); }

#define CHECK_MENO1(r,c,e) \
    if((r=c)==-1){ perror(e);return -1; }

#define MUTEX_BLOCK(mtx, code)   \
    pthread_mutex_lock(&(mtx));  \
    { code }                     \
    pthread_mutex_unlock(&(mtx));    
                                              

/**
 * @function Malloc
 * @brief malloc con controllo dell'errore
 *
 * @param size       byte di memoria da allocare
 *
 * @returns puntatore al blocco di memoria allocato
 */
void* Malloc (size_t size);

/**
 * @function Calloc
 * @brief Calloc con controllo dell'errore
 *
 * @param size       byte di memoria da allocare
 *
 * @returns puntatore al blocco di memoria allocato
 */
void* Calloc (size_t nitems, size_t size);

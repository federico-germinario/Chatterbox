/**
 * @file  util.c
 * @author Federico Germinario 545081
 * 
 */
#include <stdio.h>
#include <stdlib.h>

/**
 * @function Malloc
 * @brief malloc con controllo dell'errore
 *
 * @param size       byte di memoria da allocare
 *
 * @returns puntatore al blocco di memoria allocato
 */
void* Malloc (size_t size) {
    void * tmp;
    if ( ( tmp = malloc(size) ) == NULL) {
        perror("Malloc");
        exit(EXIT_FAILURE); }
    else
    return tmp;
}

/**
 * @function Calloc
 * @brief Calloc con controllo dell'errore
 *
 * @param size       byte di memoria da allocare
 *
 * @returns puntatore al blocco di memoria allocato
 */
void* Calloc (size_t nitems, size_t size) {
    void * tmp;
    if ( ( tmp = calloc(nitems, size) ) == NULL) {
        perror("Malloc");
        exit(EXIT_FAILURE); }
    else
    return tmp;
}

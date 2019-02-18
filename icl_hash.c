/**
 * @file icl_hash.c
 *
 * Dependency free hash table implementation.
 *  
 * @author Jakub Kurzak
 */
/* $Id: icl_hash.c 2838 2011-11-22 04:25:02Z mfaverge $ */
/* $UTK_Copyright: $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "icl_hash.h"



#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
static unsigned int
hash_pjw(void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static int string_compare(void* a, void* b) 
{
    return (strcmp( (char*)a, (char*)b ) == 0);
}

/**
 * @function lock_hash_section
 * @brief Prende la m.e. della sezione della tabella relativa a key
 *
 * @param ht      tabella hash
 * @param key     chiave appartenente alla sezione su cui prendere la
 *                mutua esclusione
 *
 * @return -1 fallimento, 0 successo
 */
int lock_hash_section(icl_hash_t *ht, void* key){
    if(!ht || !key) return -1;

    int hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    // Indice della mutex
    unsigned int ind_mutex = hash_val % ht->nsections;
    pthread_mutex_lock (&(ht->mutexes[ind_mutex]));
    return 0;
}

/**
 * @function unlock_hash_section
 * @brief Rilascia la m.e. della sezione della tabella relativa a key
 *
 * @param ht      tabella hash
 * @param key     chiave appartenente alla sezione su cui rilasciare la
 *                mutua esclusione
 *
 * @return -1 fallimento, 0 successo
 */
int unlock_hash_section(icl_hash_t *ht, void* key){
    if(!ht || !key) return -1;

    int hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    // Calcolo indice della mutex
    unsigned int ind_mutex = hash_val % ht->nsections;
    pthread_mutex_unlock (&(ht->mutexes[ind_mutex]));
    return 0;
}

/**
 * @function lock_hash
 * @brief Prende la m.e. dell' intera tabella hash
 *
 * @param ht      tabella hash
 *
 * @return -1 fallimento, 0 successo
 */
int lock_hash(icl_hash_t *ht){
    if(!ht) return -1;
    for(int i = 0; i < ht->nsections; i++){
        pthread_mutex_lock (&(ht->mutexes[i]));
    }
    return 0;
}

/**
 * @function unlock_hash
 * @brief Rilascia la m.e. dell' intera tabella hash
 *
 * @param ht      tabella hash
 *
 * @return -1 fallimento, 0 successo
 */
int unlock_hash(icl_hash_t *ht){
    if(!ht) return -1;
    for(int i = 0; i < ht->nsections; i++){
        pthread_mutex_unlock (&(ht->mutexes[i]));
    }
    return 0;
}

/**
 * @function icl_hash_create
 * @brief Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 * @param[in] hash_function -- pointer to the hashing function to be used
 * @param[in] hash_key_compare -- pointer to the hash key comparison function to be used
 *
 * @returns pointer to new hash table.
 */

icl_hash_t * icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) ){
    icl_hash_t *ht;
    int i;
    ht = (icl_hash_t*) malloc(sizeof(icl_hash_t));
    if(!ht) return NULL;
    
    ht->nentries = 0;
    ht->nbuckets = nbuckets;
    if(nbuckets >= 64) ht->nsections = nbuckets / 64;
    else {
        ht->nsections = 1;
    }
    
    ht->mutexes = (pthread_mutex_t*)malloc(ht->nsections * sizeof(pthread_mutex_t));
    ht->buckets = (icl_entry_t**)malloc(nbuckets * sizeof(icl_entry_t*));
    if(!ht->mutexes) return NULL; 
    if(!ht->buckets) return NULL; 

    for(i=0;i<ht->nbuckets;i++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;

    // Inizializzazione mutex 
    for(int i=0; i < ht->nsections; i++){
        pthread_mutex_init( &(ht->mutexes[i]), NULL );
    }

    return ht;
}

/**
 * @function icl_hash_find
 * @brief Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */

void * icl_hash_find(icl_hash_t *ht, void* key){
    icl_entry_t* curr;
    unsigned int hash_val;

    if(!ht || !key) return NULL;

    // Calcolo valore hash
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    
    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key)){
            return(curr->data);
        }

    return NULL;
}

/**
 * @function icl_hash_insert
 * @brief Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns 0 successo, -1 utente gia registrato, -2 inserimento fallito
 */

int icl_hash_insert(icl_hash_t *ht, void* key, void *data){
    icl_entry_t *curr;
    unsigned int hash_val;

    if(!ht || !key) return -2;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key)){
            return -1; /* key already exists */
    }
    /* if key was not found */
    curr = (icl_entry_t*)malloc(sizeof(icl_entry_t));
    if(!curr){
        return -2;
    }

    curr->key = key;
    curr->data = data;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;
    ht->nentries++;
    return 0;
}

/**
 * @function icl_hash_delete
 * @brief Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_delete(icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*)){
    icl_entry_t *curr, *prev;
    unsigned int hash_val;

    if(!ht || !key) return -1;
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    prev = NULL;
    for (curr=ht->buckets[hash_val]; curr != NULL; )  {
        if ( ht->hash_key_compare(curr->key, key)) {
            if (prev == NULL) {
                ht->buckets[hash_val] = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            ht->nentries--;
            free(curr);
    
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/**
 * @function icl_hash_destroy
 * @brief Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_destroy(icl_hash_t *ht, void (*free_key)(void*), void (*free_data)(void*)){
    icl_entry_t *bucket, *curr, *next;
    int i;

    if(!ht) return -1;

    // Distriggo le lock
    for(i=0; i < ht->nsections; i++)
        pthread_mutex_destroy (&(ht->mutexes[i]));
    free(ht->mutexes);

    for (i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr=bucket; curr!=NULL; ) {
            next=curr->next;
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            free(curr);
            curr=next;
        }
    }

    if(ht->buckets) free(ht->buckets);
    if(ht) free(ht);

    return 0;
}

/**
 * @file
 *
 * Header file for icl_hash routines.
 *
 */
/* $Id$ */
/* $UTK_Copyright: $ */

#ifndef icl_hash_h
#define icl_hash_h

#include <stdio.h>
#include <string.h>
#include <pthread.h>


#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

typedef struct icl_entry_s {
    void* key;
    void *data;
    struct icl_entry_s* next;
} icl_entry_t;

typedef struct icl_hash_s {
    pthread_mutex_t *mutexes;
    int nbuckets;
    int nentries;
    int nsections;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} icl_hash_t;

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
int lock_hash_section(icl_hash_t *ht, void* key);

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
int unlock_hash_section(icl_hash_t *ht, void* key);

/**
 * @function lock_hash
 * @brief Prende la m.e. dell' intera tabella hash
 *
 * @param ht      tabella hash
 *
 * @return -1 fallimento, 0 successo
 */
int lock_hash(icl_hash_t *ht);

/**
 * @function unlock_hash
 * @brief Rilascia la m.e. dell' intera tabella hash
 *
 * @param ht      tabella hash
 *
 * @return -1 fallimento, 0 successo
 */
int unlock_hash(icl_hash_t *ht);

icl_hash_t * icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );

void * icl_hash_find(icl_hash_t *, void* );

int icl_hash_insert(icl_hash_t *, void*, void *);

int icl_hash_destroy(icl_hash_t *, void (*)(void*), void (*)(void*));

int icl_hash_delete( icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );


//Permette di scorrere la tabella 
#define icl_hash_foreach(ht, dp, code)                                           \
    int tmpint;                                                                  \
    icl_entry_t *tmpent;                                                         \
    char *kp;                                                                    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++){                                \
        for (tmpent=ht->buckets[tmpint];                                         \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL);  \
             tmpent=tmpent->next){                                               \
                code                                                             \
            };                   \
    }


//Permette di scorrere la tabella in mutua esclusione 
#define icl_hash_foreach_mutex(ht, dp, code)                                     \
    int tmpint;                                                                  \
    icl_entry_t *tmpent;                                                         \
    char *kp;                                                                    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++){                                \
        unsigned int ind_mutex = tmpint % ht->nsections;                         \
        pthread_mutex_lock (&(ht->mutexes[ind_mutex]));                          \
        for (tmpent=ht->buckets[tmpint];                                         \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL);  \
             tmpent=tmpent->next){                                               \
                code                                                             \
            }pthread_mutex_unlock (&(ht->mutexes[ind_mutex]));                   \
    }

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* icl_hash_h */

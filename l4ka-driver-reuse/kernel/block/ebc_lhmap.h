/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_lhmap.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

/*
 * Header file for the linked hash map 
 */

#ifndef __LHMAP_H__
#define __LHMAP_H__

#include "block.h"
#include "server.h"
#include <linux/types.h>



typedef struct LHMapEntry {
    /* key used to identify entry */
    uint64_t key;
    /* chain list to resolve collisions */
    struct LHMapEntry *chain;
    /* double linked list entries */
    struct LHMapEntry *next;
    struct LHMapEntry *prev;
    /* user data pointer */
    void *data;
} LHMapEntry;


typedef struct {
    uint32_t entries;
    uint32_t size;
    //pthread_mutex_t lock;
    LHMapEntry *head, *tail;
    LHMapEntry *iterator;
    LHMapEntry **lhmap;
} LHMap;

/* create a new linked hash map with the specified size */
LHMap* lhmap_create(uint32_t size);

/* generic operations */
int lhmap_insert(LHMap* m, uint64_t key, void *data);
void* lhmap_lookup(LHMap* m, uint64_t key);
void* lhmap_delete(LHMap* m, uint64_t key);

/* move entry to front of list */
void* lhmap_to_front(LHMap *m, uint64_t key);
    
/* delete last entry in list */
void* lhmap_delete_last(LHMap *m, uint64_t *key);

/* get number of stored entries */
static inline uint32_t lhmap_size(LHMap *m)
{ return m->entries; }

/* some iterator functions, NOT threadsafe */
void lhmap_iterator_start(LHMap *m);
void *lhmap_iterator_next(LHMap *m, uint64_t *key);

uint32_t lhmap_force_recount(LHMap *m);

#endif /* __LHMAP_H__ */

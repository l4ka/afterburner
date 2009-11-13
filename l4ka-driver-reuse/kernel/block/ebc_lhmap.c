/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_lhmap.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

/*
 * Linked hash map implementation
 */

#include "ebc_lhmap.h"

LHMap* lhmap_create(uint32_t size)
{
    LHMap *m;
    
    dprintk(0, "Init LinkedHashMap: ");
    dprintk(0, "size %ld bytes\n", (sizeof(LHMapEntry*)*size) );

    m = (LHMap*) kmalloc(sizeof(LHMap), GFP_KERNEL);
    if(m == NULL)
    {
	panic("malloc failed");
	return NULL;
    }

    m->lhmap = (LHMapEntry**)kmalloc(sizeof(LHMapEntry*)*size, GFP_KERNEL);
    if(m->lhmap == NULL)
    {
	kfree(m);
	panic("malloc failed");
	return NULL;
    }
    
    m->head = m->tail = m->iterator = NULL;
    m->size = size;
    m->entries = 0;
    //pthread_mutex_init(&m->lock, NULL);

    dprintk(0, "done.\n");

    return m;
}

int lhmap_insert(LHMap* m, uint64_t key, void *data)
{
    LHMapEntry *e;
    uint64_t bucket = (key%m->size);

    /* create a new entry */
    if( (e=(LHMapEntry*)kmalloc(sizeof(LHMapEntry), GFP_KERNEL)) == NULL)
    {
	panic("malloc failed");
	return -1;
    }
    e->key = key;
    e->data = data;
    e->chain = NULL;

    //pthread_mutex_lock(&m->lock);

    if(m->lhmap[bucket] != NULL)
    {
	/* find end of chain */
	LHMapEntry *cur = m->lhmap[bucket];
	LHMapEntry *prev = m->lhmap[bucket];
	while(cur != NULL)
	{
	    prev = cur;
	    cur = cur->chain;
	}
	ASSERT(cur == NULL);
	ASSERT(prev != NULL);
	prev->chain = e;
    }
    else
	m->lhmap[bucket] = e;

    /* update doubly-linked list */
    if(m->head == NULL)
    {
	m->head = m->tail = e;
	e->prev = e->next = NULL;
    }
    else
    {
	e->prev = NULL;
	e->next = m->head;
	m->head->prev = e;
	m->head = e;
    }

#if 0
    /* update doubly-linked list */
    if(m->tail == NULL)
    {
	ASSERT(m->head == NULL);
	m->head = e;
	e->prev = NULL;
    }
    else
    {	
	m->tail->next = e;
	e->prev = m->tail;
    }
    
    m->tail = e;
    e->next = NULL;
#endif

    m->entries++;

    //pthread_mutex_unlock(&m->lock);
 
    return 0;
}

void* lhmap_lookup(LHMap* m, uint64_t key)
{
    uint64_t bucket = (key%m->size);

    //pthread_mutex_lock(&m->lock);

    if(m->lhmap[bucket] != NULL)
    {
	LHMapEntry *entry = m->lhmap[bucket];
	while(entry != NULL)
	{
	    if(entry->key == key) {
		//pthread_mutex_unlock(&m->lock);
		return entry->data;
	    }
	    else
		entry = entry->chain;
	}
    }

    //pthread_mutex_unlock(&m->lock);

    /* not found */
    return NULL;
}

void* lhmap_delete(LHMap* m, uint64_t key)
{
    LHMapEntry *entry, *prev;
    uint64_t bucket = (key%m->size);
    void *tmp;

    //pthread_mutex_lock(&m->lock);

    /* find entry */
    entry = m->lhmap[bucket];
    prev = NULL;
    while(entry != NULL)
    {
	if(entry->key == key)
	    break;
	else {
	    prev = entry;
	    entry = entry->chain;
	}
    }

    if(entry == NULL)
    {
	//pthread_mutex_unlock(&m->lock);
	return NULL;
    }

    /* update collision list */
    if(entry == m->lhmap[bucket]) {
	m->lhmap[bucket] = entry->chain;
    }
    else {
	ASSERT( prev->chain == entry );
	prev->chain = entry->chain;
    }

    /* update linked list */
    if(entry == m->head)
    {
	m->head = m->head->next;
	if(m->head != NULL)
	    m->head->prev = NULL;
    }
    else if(entry == m->tail)
    {
	m->tail = m->tail->prev;
	if(m->tail != NULL)
	    m->tail->next = NULL;
    }
    else
    {
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
    }

#if 0
    /* update linked list */
    if(entry->prev == NULL)
    {
	m->head = entry->next;
	entry->next->prev = NULL;
    }
    else
	entry->prev->next = entry->next;
    
    if(entry->next == NULL)
    {
	m->tail = entry->prev;
	entry->prev->next = NULL;
    }
    else
	entry->next->prev = entry->prev;
#endif

    ASSERT(m->entries > 0);
    m->entries--;

    //pthread_mutex_unlock(&m->lock);

    tmp = entry->data;

    kfree(entry);

    return tmp;
}


void* lhmap_to_front(LHMap *m, uint64_t key)
{
    LHMapEntry *entry, *prev;
    uint64_t bucket = (key%m->size);

    //pthread_mutex_lock(&m->lock);

    /* find entry */
    entry = m->lhmap[bucket];
    prev = NULL;
    while(entry != NULL)
    {
	if(entry->key == key)
	    break;
	else {
	    prev = entry;
	    entry = entry->chain;
	}
    }

    /* if entry not found or already at front do nothing */
    if(entry == NULL || entry == m->head)
    {
	//pthread_mutex_unlock(&m->lock);
	return NULL;
    }

    /* first delete entry in linked list */
    if(entry == m->tail)
    {
	m->tail = m->tail->prev;
	m->tail->next = NULL;	
    }
    else
    {
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
    }
    /* now insert at head */
    entry->prev = NULL;
    entry->next = m->head;
    m->head->prev = entry;
    m->head = entry;

#if 0
    /* update linked list */
    if(entry->prev == NULL)
	m->head = entry->next;
    else
	entry->prev->next = entry->next;
    
    if(entry->next == NULL) {
	/* entry already tail */
	ASSERT(m->tail == entry);
	return entry->data;
    }
    else
	entry->next->prev = entry->prev;

    /* move entry to front (tail in our case) */
    m->tail->next = entry;
    entry->prev = m->tail;
    entry->next = NULL;
    m->tail = entry;
#endif

    //pthread_mutex_unlock(&m->lock);
    
    return entry->data;
}


/* delete last entry in list */
void* lhmap_delete_last(LHMap *m, uint64_t *key)
{
    if(m->tail != NULL)
    {
	void *tmp = m->tail->data;
	*key = m->tail->key;
	lhmap_delete(m, m->tail->key);
	return tmp;
    }
    else
	return NULL;
}


/* some iterator functions, NOT threadsafe */
void lhmap_iterator_start(LHMap *m)
{
    m->iterator = m->head;
}

void *lhmap_iterator_next(LHMap *m, uint64_t *key)
{
    void *tmp;

    if(m->iterator != NULL)
    {
	*key = m->iterator->key;
	tmp = m->iterator->data;
	m->iterator = m->iterator->next;
	return tmp;
    }
    else
	return NULL;
}


/* go through whole lhmap and count entries */
uint32_t lhmap_force_recount(LHMap *m)
{
    uint32_t entries = 0;
    uint64_t bucket;
    LHMapEntry *entry;

    for(bucket = 0; bucket < m->size ; bucket ++)
    {
	entry = m->lhmap[bucket];
	while(entry != NULL)
	{
	    dprintk(0, "%d: B %ld, key %ld\n", entries, bucket, entry->key);
	    entry = entry->chain;
	    entries++;
	}
    }

    return entries;
}

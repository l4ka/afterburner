/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_hcache.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
/* A Hashtable Cache */

#ifndef __HCACHE_H__
#define __HCACHE_H__

#include "block.h"
#include "server.h"
#include <linux/types.h>

typedef struct HCacheEntry {
    struct HCacheEntry *chain;
    uint64_t key;
    uint64_t hd_sector;
    uint64_t fd_sector;
    uint32_t size;
    uint16_t state;
    uint32_t mask; // used to decide with sectors are valid for write entries
    struct HCacheEntry *next;
    struct HCacheEntry *prev;
} HCacheEntry;

#define ENTRY_READ	0x1
#define ENTRY_WRITE	0x2

#define HCACHE_SIZE	2995973

typedef struct HCache {
    HCacheEntry *head, *tail;
    HCacheEntry *cache[HCACHE_SIZE];
} HCache;

HCache* hcache_init(void);
int hcache_insert(HCache* c, uint64_t key, HCacheEntry *entry);
HCacheEntry* hcache_lookup(HCache* c, uint64_t key);
int hcache_delete(HCache* c, uint64_t key);
void hcache_dump(HCache* c);
HCacheEntry* hcache_get_list(HCache* c);
void hcache_iterator_start(HCache *c);
void hcache_iterator_stop(HCache *c);


#endif /* __HCACHE_H__ */

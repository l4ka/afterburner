/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_cache_mgnt.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
/*
 * Main functions for cache handling
 */


#ifndef __CACHE_MGNT_H__
#define __CACHE_MGNT_H__

#include "ebc_lhmap.h"

#define DUMP_STATS	1

typedef struct CacheInfo {
    int fd_disk;
    int fd_flash;
    int cache_size;
    uint64_t free_entries;
} CacheInfo;

/* Init caches, i.e. allocate read and write cache */
int cinit_caches(CacheInfo *info);

int cinsert_read(uint64_t key, void *data);
int cinsert_write(uint64_t key, void *data);

/* Lookup entries in read respectively write cache */
void* clookup_read(uint64_t key);
void* clookup_write(uint64_t key);

/* Lookup entry in either caches, returns first found */
void* clookup_any(uint64_t key);

/* Move entry from/to read/write cache */
int cmove_to_read(uint64_t key);
int cmove_to_write(uint64_t key);

/* Remove entry from read/write cache */
void* cremove_read(uint64_t key);
void* cremove_write(uint64_t key);

/* Get read/write cache lists */
void citerator_start_read(void);
void citerator_start_write(void);

void* citerator_next_read(uint64_t *key);
void* citerator_next_write(uint64_t *key);

/* Flush entry from read/write cache to disk */
int cflush_read(uint64_t key);
int cflush_write(uint64_t key);

/* Fetch entry from disk into read/write cache */
int cfetch_read(uint64_t key);
int cfetch_write(uint64_t key);

int cget_free_block(uint64_t *block);

void* cremove_last_read(uint64_t *key);
void* cremove_last_write(uint64_t *key);

int csize_read(void);
int csize_write(void);

#ifdef DUMP_STATS
void cdump_stats(void);
#endif


#endif /* __CACHE_MGNT_H__ */

/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     kernel/block/ebc_cache_mgnt.c
 * Description:   
 *                
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *                
 * $Id$
 *                
 ********************************************************************/

#include "energy.h"
#include "cache_mgnt.h"

#ifdef __USE_CLIST
//#include "simclist.h"
#endif

#include <list>
using namespace std;

static list<uint64_t> free_list;

extern "C" {

#ifdef DUMP_STATS
static uint32_t stat_wdeletions = 0;
static uint32_t stat_winsertions = 0;
static uint32_t stat_rdeletions = 0;
static uint32_t stat_rinsertions = 0;
#endif

static LHMap *read_cache, *write_cache;
static int fd_disk, fd_flash;

#ifdef __USE_CLIST
static list_t free_list;
#endif
static pthread_mutex_t free_lock = PTHREAD_MUTEX_INITIALIZER;


/* Init caches, i.e. allocate read and write cache */
int cinit_caches(CacheInfo *info)
{
    uint64_t i;

    read_cache = lhmap_create(info->cache_size);
    write_cache = lhmap_create(info->cache_size);

    if(read_cache == NULL || write_cache == NULL)
	return -1;

    fd_disk = info->fd_disk;
    fd_flash = info->fd_flash;

    /* initialize free block list */
    assert(free_list.empty());
#ifdef __USE_CLIST
    list_init( &free_list);
    list_attributes_copy(&free_list, list_meter_uint64_t, 1);
    list_attributes_comparator(&free_list, list_comparator_uint64_t);
#endif
    /* add all free blocks into list */
    for(i=0; i<info->free_entries; i++)
    {
	uint64_t val = i*OBJECT_SIZE;
#ifdef __USE_CLIST
	list_append(&free_list, &val);
#endif
	free_list.push_back(val);
    }

    return 0;
}


/* Insert entry in read/write cache */
int cinsert_read(uint64_t key, void *data)
{
#ifdef DUMP_STATS
    stat_rinsertions++;
#endif
    return lhmap_insert(read_cache, key, data);
}

int cinsert_write(uint64_t key, void *data)
{
#ifdef DUMP_STATS
    stat_winsertions++;
#endif
    return lhmap_insert(write_cache, key, data);
}


/* Lookup entries in read respectively write cache */
void* clookup_read(uint64_t key)
{
    return lhmap_lookup(read_cache, key);
}

void* clookup_write(uint64_t key)
{
    return lhmap_lookup(write_cache, key);
}

/* Lookup entry in either caches, returns first found */
void* clookup_any(uint64_t key)
{
    void *tmp;

    if((tmp=lhmap_lookup(read_cache, key)) != NULL)
	return tmp;
    if((tmp=lhmap_lookup(write_cache, key)) != NULL)
	return tmp;

    return NULL;
}

/* Move entry from/to read/write cache */
int cmove_to_read(uint64_t key)
{
    void *tmp;
#ifdef DUMP_STATS
    stat_wdeletions++;
    stat_rinsertions++;
#endif
    if( (tmp=lhmap_delete(write_cache, key)) == NULL) {
	ERROR_PRINT("lhmp_delete failed");
	return 1;
    }
    if(lhmap_insert(read_cache, key, tmp)) {
	ERROR_PRINT("lhmap_insert failed");
	return 1;
    }
    return 0;
}

int cmove_to_write(uint64_t key)
{
    void *tmp;
#ifdef DUMP_STATS
    stat_rdeletions++;
    stat_winsertions++;
#endif
    if( (tmp=lhmap_delete(read_cache, key)) == NULL) {
	ERROR_PRINT("lhmap_delete failed");
	return 1;
    }
    if(lhmap_insert(write_cache, key, tmp)) {
	ERROR_PRINT("lhmap_insert failed");
	return 1;
    }
    return 0;
}

/* Remove entry from read/write cache */
void* cremove_read(uint64_t key)
{
#ifdef DUMP_STATS
    stat_rdeletions++;
#endif
    return lhmap_delete(read_cache, key);
}

void* cremove_write(uint64_t key)
{
#ifdef DUMP_STATS
    stat_wdeletions++;
#endif
    return lhmap_delete(write_cache, key);
}


void citerator_start_read()
{
    lhmap_iterator_start(read_cache);
}

void citerator_start_write()
{
    lhmap_iterator_start(write_cache);
}

void* citerator_next_read(uint64_t *key)
{
    return lhmap_iterator_next(read_cache, key);
}

void* citerator_next_write(uint64_t *key)
{
    return lhmap_iterator_next(write_cache, key);
}


int cflush_read(uint64_t key)
{
    /* read entries do not have to be written back to disk, simple delete entry
     * and add it back to the free list */

    CacheEntry *entry;
    if( (entry=(CacheEntry*)cremove_read(key)) == NULL)
	return -1;
    
    pthread_mutex_lock(&free_lock);
#ifdef __USE_CLIST
    list_append(&free_list, &key);
#endif
    free_list.push_back(key);
    pthread_mutex_unlock(&free_lock);

    qemu_free(entry);
    
    return 0;
}


int cflush_write(uint64_t key)
{
    /* write entries first have to flushed back to disk */
    static uint8_t write_buffer[OBJECT_SIZE*512+1] __attribute__((aligned (512)));
    CacheEntry *entry;
    int i;

    if( (entry=(CacheEntry*)cremove_write(key)) == NULL)
	return -1;

    /* optimization for full blocks */
    if(entry->mask == BLOCK_MASK(0,OBJECT_SIZE))
    {
	if(pread64(fd_flash, write_buffer, OBJECT_SIZE*512, entry->fd_sector*512) != (OBJECT_SIZE*512))
	    ERROR_PRINT("reading block from flash failed");
	if(pwrite64(fd_disk, write_buffer, OBJECT_SIZE*512, entry->hd_sector*512) != (OBJECT_SIZE*512))
	    ERROR_PRINT("writing block to disk failed");    
    }
    else
    {
	/* go through mask and write back each entry */
	for(i=0;i<OBJECT_SIZE;i++)
	{
	    if( entry->mask & (1<<i) )
	    {
		if(pread64(fd_flash, write_buffer, 512, (entry->fd_sector+i)*512) != (512))
		    ERROR_PRINT("reading block from flash failed");
		if(pwrite64(fd_disk, write_buffer, 512, (entry->hd_sector+i)*512) != (512))
		    ERROR_PRINT("writing block to disk failed");	    
	    }
	}
    }

    pthread_mutex_lock(&free_lock);
#ifdef __USE_CLIST
    list_append(&free_list, &key);
#endif
    free_list.push_back(key);
    pthread_mutex_unlock(&free_lock);

    qemu_free(entry);

    return 0;
}

 
int cfetch_read(uint64_t key)
{
    static uint8_t read_buffer[OBJECT_SIZE*512+1] __attribute__((aligned (512)));
    uint64_t block;
    CacheEntry *entry;

    /* get a free block */
    pthread_mutex_lock(&free_lock);
#ifdef __USE_CLIST
    if(list_size(&free_list) <= 0)
#endif
	if(free_list.empty())
    {
	printf("no free blocks left\n");
	pthread_mutex_unlock(&free_lock);
	return -1;
    }
#ifdef __USE_CLIST
    block = *(uint64_t *)list_get_at(&free_list, 0);
    list_delete_at(&free_list, 0);
#endif
    block = free_list.front();
    free_list.pop_front();
    
    pthread_mutex_unlock(&free_lock);

    debug_printf("fetching %ld into cache at %ld\n", key, block);

    if( (entry=(CacheEntry*)qemu_malloc(sizeof(CacheEntry))) == NULL)
    {
	printf("malloc failed\n");
	return -1;
    }

    /* read block from disk into cache */
    if(pread64(fd_disk, read_buffer, OBJECT_SIZE*512, key*512) != (OBJECT_SIZE*512))
	printf("reading block %ld from disk failed\n", key);
    if(pwrite64(fd_flash, read_buffer, OBJECT_SIZE*512, block*512) != (OBJECT_SIZE*512))
	printf("writing block %ld to flash failed\n", block);

    entry->hd_sector = key;
    entry->fd_sector = block;
    entry->state = ENTRY_READ;
    entry->mask = BLOCK_MASK(0, OBJECT_SIZE);

    return cinsert_read(key, entry);
}

int cfetch_write(uint64_t key)
{
    ERROR_PRINT("unimplemented");
    return -1;
}


int cget_free_block(uint64_t *block)
{
    pthread_mutex_lock(&free_lock);
#ifdef __USE_CLIST
    if(list_size(&free_list) <= 0)
#endif
    	if(free_list.empty())
    {
	pthread_mutex_unlock(&free_lock);
	return -1;
    }
#ifdef __USE_CLIST
    *block = *(uint64_t *)list_get_at(&free_list, 0);
#endif
    *block = free_list.front();
    free_list.pop_front();
    pthread_mutex_unlock(&free_lock);
    return 0;
}


void* cremove_last_read(uint64_t *key)
{
    return lhmap_delete_last(read_cache, key);
}


void* cremove_last_write(uint64_t *key)
{
    return lhmap_delete_last(write_cache, key);
}


int csize_read(void)
{
    return lhmap_size(read_cache);
}

int csize_write(void)
{
    return lhmap_size(write_cache);
}

#ifdef DUMP_STATS
void cdump_stats(void)
{
    printf("Free list: %d elements\n", free_list.size());
    printf("Read Cache: %d entries, Write Cache: %d entries\n", lhmap_size(read_cache), lhmap_size(write_cache));
    printf(" Read cache stats: insertions %d, deletions %d\n", stat_rinsertions, stat_rdeletions);
    printf("Write cache stats: insertions %d, deletions %d\n", stat_winsertions, stat_wdeletions);

    stat_rinsertions = stat_rdeletions = stat_wdeletions = stat_winsertions = 0;
}


void ctemp(void)
{
    printf("Forced recount:\n");
    printf("Read Cache: %d\n", lhmap_force_recount(read_cache));
    printf("Write Cache: %d\n", lhmap_force_recount(write_cache));
}

} /* extern "C" */

#endif


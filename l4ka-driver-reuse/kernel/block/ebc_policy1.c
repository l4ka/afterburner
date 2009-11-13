/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_policy1.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

/* Policy 1: Write-log, Cache, inclusive model */

#include "ebc_energy.h"
#include "ebc_cache_mgnt.h"
#include "ebc_lhmap.h"
#include "ebc_prefetcher.h"
#include "ebc_lru.h"
#include "ebc_tools.h"

/*
 * ***************
 * CONFIG SECTION :
 */
#define HASH_CACHE_SIZE		2995973
#define TIMER_INTERVALL		1000
#define IDLE_TRESHOLD		5000
#define STANDBY_THRESHOLD	80000
#define DISK_ACTIVE		0
#define DISK_STANDBY		1
#define FLUSH_CACHE		1
/* 
 * CONFIG END 
 *****************
 */

//static float load_threshold = 0.5f; // 50% of io_max
static int load_at_once = 30;
static const uint32_t lru_cache_size = 32768;
/* *************** */

static EnergyState p1e;
//static QEMUTimer *ptimer;

#ifdef DUMP_STATS
#define STATS_INTERVALL		10000
//static QEMUTimer *stimer;
static uint32_t stats_cache_hits = 0;
static uint32_t stats_cache_misses = 0;
#define INC_HIT	stats_cache_hits++;
#define INC_MISS stats_cache_misses++;
#else
#define INC_HIT
#define INC_MISS
#endif

static int64_t last_access, last_cload, last_wflush, standby_time;
static uint8_t disk_state; // active(0) or standby(1)
static uint32_t standby_seconds, standby_counts;

//static pthread_mutex_t lru_lock = PTHREAD_MUTEX_INITIALIZER;
//static pthread_t cache_thread, flush_thread;
//static pthread_attr_t cache_attr;


/* structure to store IO statistics */
static struct {
    uint32_t io_count;
    uint32_t io_ops;
    uint32_t io_max;
} io_stat;


/* 
 * TODO:
 */


static void disk_activity(void)
{
    BUG();
    L4_KDB_Enter("Implement me");

#if 0
    io_stat.io_count++;
    last_access = qemu_get_clock(rt_clock);
    if(disk_state == DISK_STANDBY) {
	disk_state = DISK_ACTIVE;
	dprintk(0,"Disk in standby for %ld seconds\n", (qemu_get_clock(rt_clock)-standby_time)/1000);
	standby_seconds += ((qemu_get_clock(rt_clock)-standby_time)/1000);
	standby_counts++;
    }
#endif
}

/*
 * Takes the LRU list and loads all entries into the cache if not already
 * there
 */
static void energy_cache_loader(void *opaque)
{
    uint32_t max_transitions = io_stat.io_max * load_at_once / 100;
    uint32_t transitions;

    dprintk(0,"ECL: loading...\n");

    /* go through LRU list and load entries into cache */
    //pthread_mutex_lock(&lru_lock);

    transitions = lru_load_cache(max_transitions);

#if FLUSH_CACHE
    /* some transitions left, flush write log */
    if(transitions < max_transitions)
    {
	CacheEntry *entry;
	uint64_t key;
	citerator_start_write();
	while( ((entry=(CacheEntry*)citerator_next_write(&key)) != NULL)
	       && (transitions < max_transitions))
	{
	    if(cflush_write(key))
		dprintk(0,"Write back for key %ld failed!\n", key);

	    transitions++;
	}
	
    }

    /* if cache contains more entries than A1in + Am flush last entries */
    if(csize_read() > lru_cache_size)
    {
	uint64_t key;
	cremove_last_read(&key);
    }
#endif

    //pthread_mutex_unlock(&lru_lock);
    //pthread_exit(NULL);
}





static void energy_timer(void *opaque)
{
    BUG();
    L4_KDB_Enter("Implement me");

#if 0
    void *status;
    int64_t current = qemu_get_clock(rt_clock);
    int64_t elapsed = current - last_access;

    /* update io stats */
    io_stat.io_ops = io_stat.io_count/(TIMER_INTERVALL/1000);
    if(io_stat.io_ops > io_stat.io_max)
	io_stat.io_max = io_stat.io_ops;
    io_stat.io_count = 0;
    
    /* transfer entries from LRU list into cache and flush write log*/
    if( (current-last_cload > IDLE_TRESHOLD) && (disk_state==DISK_ACTIVE) && 
	(io_stat.io_ops < (io_stat.io_max*load_threshold)) )
    {
	/* create extra thread for cache loading */
	pthread_create(&cache_thread, &cache_attr, energy_cache_loader, NULL);
	last_cload = qemu_get_clock(rt_clock);
    }

    /* standby hard disk */
    if( (elapsed > STANDBY_THRESHOLD) && (disk_state == DISK_ACTIVE))
    {
	/* 
	 * wait for cache thread to finish, otherwise disk could be woken up by
	 * cache thread
	 */
	pthread_join(cache_thread, &status);

	dprintk(0,"Transfering disk to standby state\n");
	disk_state = DISK_STANDBY;
	standby_time = qemu_get_clock(rt_clock);
	hdstandby(p1e.fd_disk);
    }

    qemu_mod_timer(ptimer, qemu_get_clock(rt_clock) + TIMER_INTERVALL);
#endif
    
}

#ifdef DUMP_STATS
static void stat_timer(void *opaque)
{
    BUG();
    L4_KDB_Enter("Implement me");
    
#if 0
    dprintk(0,"=== STATS ===\n");
    cdump_stats();
    lru_dump_stats();
    dprintk(0,"Hits: %u, Misses: %u\n", stats_cache_hits, stats_cache_misses);
    dprintk(0,"=============\n");
    stats_cache_hits = stats_cache_misses = 0;
    qemu_mod_timer(stimer, qemu_get_clock(rt_clock) + STATS_INTERVALL);
#endif
}
#endif

int energy_init(int fd_disk, int fd_flash)
{
    int64_t fsect;
    CacheInfo cinfo;

    p1e.fd_disk = cinfo.fd_disk = fd_disk;
    p1e.fd_flash = cinfo.fd_flash = fd_flash;

    ASSERT(sizeof( ((CacheEntry*)0)->mask)*8 >= OBJECT_SIZE);

    BUG();
    L4_KDB_Enter("Implement me");
#if 0
    fsect = lseek(p1e.fd_flash, 0, SEEK_END)/512;
#endif
    cinfo.cache_size = HASH_CACHE_SIZE;

    cinfo.free_entries = (fsect/OBJECT_SIZE)-1;

    /* initialize hash table caches */    
    if(cinit_caches(&cinfo))
    {
	dprintk(0,"Caches init failed\n");
	return 1;
    }

    /* initialize LRU list */
    dprintk(0,"Init LRU list ");
    if( lru_init() )
    {
	dprintk(0,"lru cache init failed\n");
	return 1;
    }

    /* initialize prefetcher */
    if( prefetcher_init() )
    {
	dprintk(0,"prefetcher init failed\n");
	return 1;
    }

    dprintk(0,"done.\n");


    /* create new qemu timer and attach our timer function */
    BUG();
    L4_KDB_Enter("Implement me");
#if 0
    ptimer = qemu_new_timer(rt_clock, energy_timer, NULL);
    qemu_mod_timer(ptimer, qemu_get_clock(rt_clock) + TIMER_INTERVALL);
    last_access = last_cload = last_wflush = qemu_get_clock(rt_clock);
#ifdef DUMP_STATS
    stimer = qemu_new_timer(rt_clock, stat_timer, NULL);
    qemu_mod_timer(stimer, qemu_get_clock(rt_clock) + STATS_INTERVALL);
#endif

    /* init thread attributes as joinable */
    pthread_attr_init(&cache_attr);
    pthread_attr_setdetachstate(&cache_attr, PTHREAD_CREATE_JOINABLE);
#endif
    
    io_stat.io_count = io_stat.io_ops = io_stat.io_max = 0;
    standby_seconds = standby_counts = 0;
    dprintk(0,"energy_init(): done\n");
    return 0;
}

int64_t energy_getlength(void)
{
    int64_t dsize, fsize;

    BUG();
    L4_KDB_Enter("Implement me");
#if 0
    dsize = lseek(p1e.fd_disk, 0, SEEK_END);
    fsize = lseek(p1e.fd_flash, 0, SEEK_END);
#endif
    return dsize;
}

/*
 * Main function for read. Check for the following conditions:
 * 
 * - Entry not in cache, read from disk.
 *
 * - Entry in cache, check boundary and if failed (only for write entries)
 *    flush write and read from disk, otherwise read from flash.
 * 
 * Always updates LRU hot list
 */
int energy_readtranslation(EnergyRequest *req)
{
    CacheEntry *entry;
    uint64_t key = (req->sector_num & OBJECT_MASK);
    int offset = req->sector_num % OBJECT_SIZE;

    /* update lru */
    lru_handle_read(key);

    /* lookup request in read cache */
    if((entry=(CacheEntry*)clookup_read(key)) != NULL)
    {
	INC_HIT
	dprintk(0,"Handling req %ld through readcache => ", req->sector_num);
	req->fd = FD_FLASH;
	req->sector_num = entry->fd_sector + offset;
	dprintk(0,"%ld\n", req->sector_num);
	return FD_FLASH;
    }

    /* lookup request in write cache */
    if((entry=(CacheEntry*)clookup_write(key)) != NULL)
    {
	uint32_t req_mask = BLOCK_MASK(offset, req->size);

	/* check boundaries */
	if(req_mask & entry->mask)
	{
	    INC_HIT
	    dprintk(0,"Handling req %ld through cache => block %ld, offset %d\n",
		   req->sector_num, key, offset);
	    req->fd = FD_FLASH;
	    req->sector_num = entry->fd_sector + offset;
	    ASSERT( (offset + req->size) <= OBJECT_SIZE);
	    return FD_FLASH;	    
	}
	else
	{
	    INC_MISS
	    dprintk(0,"Read boundary check failed: %ld,%d vs %ld,%u\n", req->sector_num,
		   req->size, entry->hd_sector, entry->mask);
	    /* if we stored a modified entry in the write log, flush it back */	    
	    dprintk(0,"Modified, writing back first.\n");
	    disk_activity();	    
	    if(cflush_write(key))
		panic("flush failed\n");
	    req->fd = FD_DISK;
	    return FD_DISK;
	}
    }
    INC_MISS
    req->fd = FD_DISK;
    disk_activity();
    return FD_DISK;
}

/*
 * Main function for writes. Checks for the following conditions: 
 *
 * - If entry in read cache, use it to write to and move it to write cache
 * - If entry in write cache, use it to write to and update mask
 * - If entry in none of the two caches, write directly to disk, only in standby write to log
 * 
 */
int energy_writetranslation(EnergyRequest *req)
{
    uint64_t key = (req->sector_num & OBJECT_MASK);
    int offset = req->sector_num % OBJECT_SIZE;
    CacheEntry *entry;

    /* lookup entry in read cache */
    if((entry=(CacheEntry*)clookup_read(key)) != NULL)
    {
	dprintk(0,"WA: %ld: read entry -> write entry\n", req->sector_num);
	req->fd = FD_FLASH;
	req->sector_num = entry->fd_sector + offset;
	entry->state = ENTRY_WRITE;
	cmove_to_write(key);
	return FD_FLASH;
    }

    /* lookup entry in write cache and update mask*/
    if((entry=(CacheEntry*)clookup_write(key)) != NULL)
    {
	dprintk(0,"WA: %ld: update write entry\n", req->sector_num);
	dprintk(0,"    old mask: %x ", entry->mask);
	req->fd = FD_FLASH;
	req->sector_num = entry->fd_sector + offset;
	entry->mask |= BLOCK_MASK(offset,req->size);
	dprintk(0," new mask: %x (size=%d)\n", entry->mask, req->size);
	return FD_FLASH;
    }

    /* no entry found, write to disk if active, in standby write to log */
    if(disk_state == DISK_STANDBY)
    {
	uint64_t block;

	dprintk(0,"WA: %ld: new entry\n", req->sector_num);
	/* get free block */
	if(!cget_free_block(&block))
	{
	    if( (entry = (CacheEntry*)kmalloc(sizeof(CacheEntry), GFP_KERNEL)) == NULL)
	    {
		panic("malloc failed");
		req->fd = FD_DISK;
		return FD_DISK;
	    }
	    
	    entry->hd_sector = key;
	    entry->fd_sector = block;
	    entry->mask = BLOCK_MASK(offset, req->size);
	    entry->state = ENTRY_WRITE;
	    dprintk(0,"    mask: %x (size=%d)\n", entry->mask, req->size);
	    
	    if(cinsert_write(key, entry))
		panic("insert failed");
	    
	    req->fd = FD_FLASH;
	    req->sector_num = entry->fd_sector + offset;
	    return FD_FLASH;
	}
	else
	{
	    dprintk(0,"no free blocks left, writing to disk\n");
	    req->fd = FD_DISK;
	    disk_activity();
	    return FD_DISK;
	}	
	panic("BUG");
	ASSERT(0);
    }
    else // disk active, write directly to disk
    {
	req->fd = FD_DISK;
	return FD_DISK;
    }

// we should not come here
    return 0;
}


int energy_shutdown(void)
{
    CacheEntry *entry;
    uint64_t key;
    int i=0;
    void *status;

    dprintk(0,"Shutdown requested!\n");

    BUG();
    L4_KDB_Enter("Implement me");
#if 0
    qemu_del_timer(ptimer);
#ifdef DUMP_STATS
    qemu_del_timer(stimer);
#endif

    pthread_join(cache_thread, &status);
#endif
    
    cdump_stats();
//    ctemp();

    dprintk(0,"Flushing write cache");
    citerator_start_write();

    while( (entry=(CacheEntry*)citerator_next_write(&key)) != NULL)
//    while( cremove_last_write() != NULL)
    {
//	dprintk(0,"%d: WB key %ld\n", i, key);
	if(cflush_write(key))
	    dprintk(0,"Write back for key %ld failed!\n", key);
	dprintk(0,".");
	i++;
    }
    dprintk(0,"done\n");
    dprintk(0,"Flushed %d entries\n", i);

    dprintk(0,"Disk %d times in standby, for overall %d seconds\n", standby_counts, standby_seconds);
    
    return 0;
}


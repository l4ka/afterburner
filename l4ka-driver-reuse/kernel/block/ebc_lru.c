/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_lru.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "ebc_lru.h"
#include "ebc_lhmap.h"
#include "ebc_cache_mgnt.h"
#include "ebc_prefetcher.h"


/*
 * Configuration parameters for the 2Q algorithm
 */
static const uint32_t tq_cache_size = 32768;
static const uint32_t tq_kin_divisor = 4; // 25%
static const uint32_t tq_kout_divisor = 2; // 50%
static const uint32_t min_transitions = 500;
/* Some derived values */
static  uint32_t tq_ka = 0;
static  uint32_t tq_kin = 0;
static  uint32_t tq_kout= 0;


/* 
 * Three data structures used to hold A1in, A1out, Am 
*/
static LHMap *a1in, *a1out, *am;
/*
 * Corresponding lhmap sizes
 */
static const uint32_t a1in_size = 11717;
static const uint32_t a1out_size = 23417;
static const uint32_t am_size = 46817;

static int lru_dummy;
int lru_2q_load_am(char *filename);

int lru_init(void)
{
    tq_kin = (tq_cache_size/tq_kin_divisor);
    tq_kout= (tq_cache_size/tq_kout_divisor);
    tq_ka = tq_cache_size - tq_kin;

    if( (a1in = lhmap_create(a1in_size)) == NULL)
    {
	panic("Failed to create A1in cache");
	return -1;
    }
    if( (a1out = lhmap_create(a1out_size)) == NULL)
    {
	panic("Failed to create A1out cache");
	return -1;
    }
    if( (am = lhmap_create(am_size)) == NULL)
    {
	panic("Failed to create Am cache");
	return -1;
    }

    if(lru_2q_load_am("hot2q.dat"))
    {
	dprintk(0,  "\n!!! WARNING: Running with cold cache !!!\n");
    }
    
    return 0;
}


int lru_load_cache(uint32_t max)
{
    uint64_t key;
    uint32_t transitions = 0;

    if(max < min_transitions)
	max = min_transitions;

    lhmap_iterator_start(am);
    while( (lhmap_iterator_next(am, &key) != NULL) && (transitions < max))
    {
	if(clookup_any(key) == NULL)
	{
	    if(cfetch_read(key))
	    {
		dprintk(0,"fetching of key %ld failed\n", key);
		break;
	    }
	    transitions++;

	    transitions += prefetcher_load_entries(key, 10);
	}
    }

    return transitions;
}

static void lru_reclaim(void)
{
    uint64_t key;
    
    /* balance queues */
    if(lhmap_size(a1in) > tq_kin)
    {
	/* page out tail of A1in and add it to head of A1out */
	if(lhmap_delete_last(a1in, &key) == NULL)
	{
	    panic("Delete from A1in failed");
	    return;
	}
	if(lhmap_insert(a1out, key, &lru_dummy) )
	{
	    panic("Insert in A1out failed");
	    return;
	}
	/* if a1out is full, remove tail */
	if(lhmap_size(a1out) > tq_kout)
	{
	    if(lhmap_delete_last(a1out, &key) == NULL)
	    {
		panic("Delete from A1out failed");
		return;
	    }
	}
    }

    if(lhmap_size(am) > tq_ka)
    {
	/* page out tail of Am */
	if(lhmap_delete_last(am, &key) == NULL)
	{
	    panic("Delete from Am failed");
	    return;
	}
    }

}

void lru_handle_read(uint64_t key)
{
    /* X is in Am, move to head of Am */
    if(lhmap_to_front(am, key) == NULL)
    {
	/* X is in A1out, move to head of Am */
	if(lhmap_delete(a1out, key) != NULL)
	{
	    lhmap_insert(am, key, &lru_dummy);
	    lru_reclaim();
	}
	/* X in A1in, do nothing */
	else if(lhmap_lookup(a1in, key) != NULL)
	{
//	    lhmap_insert(am, key, &lru_dummy);
//	    lru_reclaim();
	}
	/* X in no queue, add to A1in */
	else
	{
	    lhmap_insert(a1in, key, &lru_dummy);
	    lru_reclaim();
	}
    }

}


void lru_dump_stats(void)
{
    dprintk(0,"Am size: %u, A1out size: %u, A1in size: %u\n", lhmap_size(am), 
	   lhmap_size(a1out), lhmap_size(a1in));
}

int lru_2q_load_am(char *filename)
{
    
    BUG();
    L4_KDB_Enter("Implement me");
#if 0
    int entries = 0;
    uint64_t key;
    
    FILE *out;

    if( (out=fopen(filename, "r")) == NULL)
    {
	dprintk(0,  "Cannot open file %s\n", filename);
	return 1;
    }
    while (!feof(out) && (entries < tq_cache_size))
    {
	if(fscanf(out, "%llu\n", &key) != 1)
	    dprintk(0,"Invalid 2q file\n");
	lhmap_insert(am, key, &lru_dummy);
	entries++;
    }
    dprintk(0,"2Q: Loaded %d entries\n", entries);
    fclose(out);
#endif
    return 0;
}

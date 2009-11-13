/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_prefetcher.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "ebc_prefetcher.h"
#include "ebc_lhmap.h"
#include "ebc_cache_mgnt.h"

static const uint32_t initial_prefetch_degree = 16;
static const uint32_t initial_trigger_distance = 4;

typedef struct PEntry{
    uint64_t next;
} PEntry;

static LHMap *prefetch_cache;

int prefetcher_init(void)
{
    BUG();
    L4_KDB_Enter("Implement me");

#if 0
    uint64_t source, target;
    FILE *pdata;
    PEntry *entry;

    dprintk(0,"Initializing prefetch cache ");

    if( (prefetch_cache = lhmap_create(2097169)) == NULL)
	return -1;

    /* load prefetch data from file into cache */
    if( (pdata=fopen("prefetch.data", "rb")) == NULL)
    {
	ERROR_PRINT("cannot open prefetch.data");
	return -1;
    }

    while(fscanf(pdata, "%llu -> %llu\n", &source, &target ) != EOF)
    {
	if(lhmap_lookup(prefetch_cache, source) != NULL)
	{
	    ERROR_PRINT("invalid prefetcher data");
	    return -1;
	}

	if( (entry = (PEntry*)qemu_malloc(sizeof(PEntry))) == NULL)
	{
	    ERROR_PRINT("malloc failed");
	    return -1;
	}

	entry->next = target;
	
	if( lhmap_insert(prefetch_cache, source, entry))
	{
	    ERROR_PRINT("insert failed");
	    return -1;
	}
    }


    fclose(pdata);

#endif
    dprintk(0,"done.\n");

    return 0;
}


int prefetcher_load_entries(uint64_t base, uint32_t count)
{
    PEntry *entry;
    int i;
    uint64_t cur_block = base;

//    putchar('P');

    for(i=0;i<count;i++)
    {
	if( (entry=(PEntry*)lhmap_lookup(prefetch_cache, cur_block)) == NULL)
	    return i;

	cur_block = entry->next;

	/* load only if not already in cache */
	if(clookup_any(cur_block) == NULL)
	{
	    if(cfetch_read(cur_block))
	    {
		dprintk(0,"prefetching block failed\n");
		return i;
	    }
	}
//	putchar('.');
    }

    return count;
}


#include "ebc_hcache.h"

// should be a prime number, at least '#max_elements/0.7' large
// for 32GB and 16KB elements: min 2995932 => 2995973 is next prime
#define HCACHE_SIZE	2995973

//HCacheEntry *cache[HCACHE_SIZE];
//static pthread_mutex_t hcache_lock = PTHREAD_MUTEX_INITIALIZER;
//static HCacheEntry *read_head, *read_tail;
//static HCacheEntry *write_head, *write_tail;

HCache* hcache_init(void)
{
    HCache *cache;
    dprintk(0, "Init Cache: ");

    dprintk(0, "Cache is %ld bytes\n", sizeof(HCache));

    cache = kmalloc(sizeof(HCache), GFP_KERNEL);
    if(cache == NULL)
    {
      dprintk(0, "hcache_init(): malloc failed.\n");
      return NULL;
    }

    cache->head = cache->tail = NULL;
    //pthread_mutex_init(&cache->lock, NULL);

    dprintk(0, "done.\n");

    return cache;
}

/*
 * Insert a single entry into the hash table.
 * rw specifies if this is a read(0) or write(>0) entry
 */
int hcache_insert(HCache *c, uint64_t key, HCacheEntry *entry)
{
    uint64_t bucket = (key%HCACHE_SIZE);

    /* make sure chain is always terminated */
    entry->chain = NULL;

    //pthread_mutex_lock(&c->lock);

    if(c->cache[bucket] != NULL)
    {
	/* find end of chain */
	HCacheEntry *cur  = c->cache[bucket];
	HCacheEntry *prev = c->cache[bucket];
	while(cur != NULL)
	{
	    prev = cur;
	    cur = cur->chain;
	}
	ASSERT(cur == NULL);
	ASSERT(prev != NULL);
	prev->chain = entry;
    }
    else
	c->cache[bucket] = entry;


    /* update doubly-linked list */
    if(c->head == NULL)
	c->head = entry;
    else
	c->tail->next = entry;
    
    entry->prev = c->tail;
    c->tail = entry;
    
    entry->next = NULL;

    //pthread_mutex_unlock(&c->lock);
    return 0;
}


HCacheEntry* hcache_lookup(HCache* c, uint64_t key)
{
    uint64_t bucket = (key%HCACHE_SIZE);

    //pthread_mutex_lock(&c->lock);

    if(c->cache[bucket] != NULL)
    {
	HCacheEntry *entry = c->cache[bucket];
	while(entry != NULL)
	{
	    if(entry->key == key) {
		//pthread_mutex_unlock(&c->lock);
		return entry;
	    }
	    else
		entry = entry->chain;
	}
    }

    //pthread_mutex_unlock(&c->lock);

    /* not found */
    return NULL;
}


int hcache_delete(HCache *c, uint64_t key)
{
    HCacheEntry *entry, *prev;
    uint64_t bucket = (key%HCACHE_SIZE);

    //pthread_mutex_lock(&c->lock);

    /* find entry */
    entry = c->cache[bucket];
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
	//pthread_mutex_unlock(&c->lock);
	return -1;
    }

    /* update collision list */
    if(entry == c->cache[bucket]) {
	c->cache[bucket] = entry->chain;
    }
    else {
	ASSERT( prev->chain == entry );
	prev->chain = entry->chain;
    }

    /* update linked list */
    if(entry->prev == NULL)
	c->head = entry->next;
    else
	entry->prev->next = entry->next;
    
    if(entry->next == NULL)
	c->tail = entry->prev;
    else
	entry->next->prev = entry->prev;

    //pthread_mutex_unlock(&c->lock);
    return 0;
}

void hcache_iterator_start(HCache *c)
{
}

void hcache_iterator_stop(HCache *c)
{
}

HCacheEntry* hcache_get_list(HCache* c)
{
    return c->head;
}

void hcache_dump(HCache* c)
{
    int i;
    for(i=0;i<HCACHE_SIZE;i++)
    {
	HCacheEntry *entry = c->cache[i];
	while(entry != NULL)
	{
	    dprintk(0, "Bucket %d, hd %ld, fd %ld, size %d\n", i, entry->hd_sector, entry->fd_sector, entry->size);
	    entry = entry->chain;
	}
    }
    
}

/*********************************************************************
 *                
 * Copyright (C) 2006-2008,  Karlsruhe University
 *                
 * File path:     l4ka/memory.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __L4KA__MEMORY_H__
#define __L4KA__MEMORY_H__

#include <debug.h>
#include INC_WEDGE(vcpulocal.h)
#include INC_ARCH(cpu.h)


#if defined(CONFIG_L4KA_VMEXT)
class ptab_info_t
{
private:

    static const word_t ld_max_entries = (PAGE_BITS);
    static const word_t max_entries = (1UL << ld_max_entries);
    pgent_t *ptab_bp[max_entries];
    pgent_t *invalid;
    pgent_t *cleared;
    

    word_t hash_ptab_addr( word_t ptab_addr, word_t count, word_t &h1, word_t &h2 )
	{ 
	    /* Double multiplicative hashing a la Knuth
	     * with A = ((sqrt(5)-1)/2) and s = A * 2^32 
	     */
	    
	    if (count) 
	    {
		//printf( "a " << (void *) ptab_addr 
		//  << " c " << count
		//  << " h1 " << h1 
		//  << " h2 " << h2
		//  << "\n");
		return (h1 + count * h2) & (max_entries-1);
	    }
	    
	    const word_t s = 2654435769U;
	    word_t key = (ptab_addr >> PAGE_BITS);
	    h1 = h2 = (s * key);
	    
	    h1 >>= 32 - ld_max_entries;
	    h2 >>= (32 - (2 * ld_max_entries));
	    h2 &= max_entries-1;
	    h2 |= 1;
	    
	    // printf( "a " << (void *) ptab_addr 
	    // << " h1 " << h1 
	    // << " h2 " << h2
	    // << "\n");
	    
	    return h1;
	}

    word_t dbg_set;
    
    void dbg(const char *prefix, const char *info, pgent_t **cur, word_t ptab_addr, pgent_t *pdir, 
	    word_t count, bool force=false)
	{
	    static const word_t dbg_threshold = 5;
	    static word_t dbg_count = 0;
	    
	    dbg_count++;

	    if (count > dbg_threshold)
	    {
		printf(prefix);
		printf("ba %x ma %x bp %x mp %x ci %x c %d s %x p %d %s\n", 
		       ((*cur < invalid) ? (*cur)->get_address() : NULL),
		       ptab_addr, *cur, pdir, cur - ptab_bp, count, dbg_set,
		       dbg_count, info);
		
		dbg_count = 0;
	    }
	}
	    
    void set( word_t ptab_addr, pgent_t *pdent)
	{
	    word_t count = 0;
	    word_t h1, h2;
	    pgent_t **cur = NULL;
	    
	    for (;;)
	    {
		cur = &ptab_bp[hash_ptab_addr(ptab_addr, count, h1, h2)];
		
		if (*cur >= invalid)
		{
		    /* invalid or cleared entry: break and use */
		    break;
		}
		else if ( (*cur)->get_address() == ptab_addr)
		    dbg("spd", "shar", cur, ptab_addr, pdent, count, true);
		
		
		//dbg("spd", "test", cur, ptab_addr, pdent, count);
		if (++count == max_entries)
		{
		    ASSERT(false);
		    return;
		}
		
	    }
	    
	    /* 
	     * We'll do FIFO here, by swapping the most recent and the 
	     */
	    
	    dbg("spd", "done", cur, ptab_addr, pdent, count);

	    *cur = pdent; 
	    
	    dbg_set++;
	}

    void clear( word_t ptab_addr, pgent_t *pdent)
	{
	    word_t count = 0;
	    word_t h1, h2;
	    pgent_t **cur = NULL;
	    
	    for (;;)
	    {
		cur = &ptab_bp[hash_ptab_addr(ptab_addr, count, h1, h2)];
		if (*cur == invalid)
		{
		    dbg("cpd", "invl", cur, ptab_addr, pdent, count, true);
		    /* invalid entry: return */
		    return;
		}
		
		if (*cur == pdent)
		{
		    /* found entry: invalidate and return */
		    dbg("cpd", "done", cur, ptab_addr, pdent, count);
		    *cur = cleared;
		    dbg_set--;
		    return;
		}

		/* other entry: continue */
		//dbg("cpd", "test", cur, ptab_addr, pdent, count);
				
		if (++count == max_entries)
		{
		    ASSERT(false);
		    return;
		}
	    }
	}

    bool get( word_t ptab_addr, pgent_t **pdent )
	{ 

	    word_t count = 0;
	    word_t h1, h2;
	    pgent_t **cur = NULL;
	    
	    for (;;)
	    {
		cur = &ptab_bp[hash_ptab_addr(ptab_addr, count, h1, h2)];
		if (*cur <= invalid && (*cur)->get_address() == ptab_addr)
		{
		    /* found entry: return */
		    dbg("gpd", "done", cur, ptab_addr, 0, count);
		    *pdent = *cur;
		    return true;
		}
		
		else if (*cur == invalid)
		{
		    /* invalid entry: we have already deallocated the AS */
		    dbg("gpd", "invl", cur, ptab_addr, 0, count, true);
		    return false;
		}

		//dbg("gpd", "test", cur, ptab_addr, 0, count);
		
		if (++count == max_entries)
		{
		    ASSERT(false);
		    return false;
		}
	    }
	}

public:
    
    ptab_info_t()
	{
	    invalid = (pgent_t *) 0xfffffffe;
	    cleared = (pgent_t *) 0xffffffff;
	    
	    for (word_t i=0; i < max_entries; i++)
		ptab_bp[i] = invalid;
	    
	    dbg_set = 0;
	}


    void update(pgent_t *pdent, pgent_t new_val)
	{
	    if (pdent == NULL)
	    {
		printf( "ptab_info update: pdir NULL\n");
		return;
	    }
	    
	    if ( pdent->get_raw() == new_val.get_raw())
		return;
    
	    if( pdent->is_valid() && !pdent->is_superpage() )
		clear(pdent->get_address(), pdent);
	    
	    if ( new_val.is_valid() && !new_val.is_superpage())
		set(new_val.get_address(), pdent);

	}
    
    void clear(word_t pdir_paddr)
	{	
	    pgent_t *pdir =  (pgent_t *) (pdir_paddr + get_vcpu().get_kernel_vaddr());
	    
	    for (word_t i=0; i < max_entries; i++)
	    {
		pgent_t *cur_pdir = (pgent_t *) ((word_t) ptab_bp[i] & PAGE_MASK);
		if (cur_pdir == pdir)
		{
		    dbg("cpd", "tdel", &ptab_bp[i], 0, pdir, i, true);
		    ptab_bp[i] = cleared;
		    dbg_set--;
		}
	    }
	}

    
    bool retrieve(pgent_t *pgent, pgent_t **pdir )
	{
	    word_t ptab_addr = (word_t) pgent - get_vcpu().get_kernel_vaddr();
	    ptab_addr &= PAGE_MASK;
	    return get( ptab_addr, pdir );
	}
};
extern ptab_info_t ptab_info;
#endif

INLINE L4_Word_t unmap_device_mem( L4_Word_t addr, L4_Word_t bits, L4_Word_t rwx=L4_FullyAccessible)
{
    L4_Word_t ret = 0;
    
    for (word_t pt=0; pt < (1UL << bits) ; pt+= PAGE_SIZE)
    {
	L4_Fpage_t fp = L4_FpageLog2( addr + pt, PAGE_BITS);
	word_t old_attr = 0;

	if (contains_device_mem(addr + pt, addr + pt + PAGE_SIZE - 1))
	{
	    old_attr = 7;
	    backend_unmap_device_mem(addr + pt, PAGE_SIZE, old_attr);		
	}
	else
	    old_attr = L4_Rights(L4_Flush( fp + rwx ));
	    
	ret |= old_attr;
    }
    return ret;
}

static const L4_Word_t invalid_pdir_idx = 1024+1;

class unmap_cache_t
{
private:
    static const L4_Word_t cache_size = 60;
    L4_Fpage_t fpages[cache_size];
    L4_Word_t count;
    bool flush;
    
#if defined(CONFIG_VSMP) && !defined(CONFIG_SMP_ONE_AS)
    void lock() { thread_mgmt_lock.lock(); }
    void unlock() { thread_mgmt_lock.unlock(); }
#else
    void lock() {  }
    void unlock() {  }
#endif    
public:
    void add_mapping( pgent_t *pgent, L4_Word_t bits, pgent_t *pdent, 
	    L4_Word_t rwx=L4_FullyAccessible, bool do_flush=false )
	{
	    /* Check if device memory */
	    vcpu_t &vcpu = get_vcpu();
	    word_t paddr = pgent->get_address();
	    word_t kaddr = pgent->get_address() + vcpu.get_kernel_vaddr();
	    
	    if (contains_device_mem(paddr, paddr + (1UL << bits) - 1))
	    {
		dprintf(debug_device, "unmap device_mem addr %x bits %x paddr %x ra %x\n",
			kaddr, bits, paddr, __builtin_return_address((0)));
		unmap_device_mem(paddr, bits, rwx);
		return;
	    }
#if defined(CONFIG_VSMP) && !defined(CONFIG_SMP_ONE_AS)
	    if (vcpu_t::nr_vcpus > 1)
	    {
		if (pdent == NULL)
		    ptab_info.retrieve(pgent, &pdent );
		
		word_t pdir_idx = (((word_t) pdent) & ~PAGE_MASK) >> 2;
		word_t ptab_idx = pgent->is_superpage() ? 0 : (((word_t) pgent) & ~PAGE_MASK) >> 2;	
		word_t vaddr = (pdir_idx << PAGEDIR_BITS) | (ptab_idx << PAGE_BITS); 
		word_t pdir_paddr = (((word_t) pdent) & PAGE_MASK) - vcpu.get_kernel_vaddr();
		task_info_t *ti = get_task_manager().find_by_page_dir( pdir_paddr );

		dprintf(debug_unmap, "add page vaddr %x kaddr %x  paddr %x cr3 %x ti %x ra %x\n",
			    vaddr, kaddr, paddr, pdir_paddr, ti,  __builtin_return_address((0)));


		if (ti)
		{
		    ti->add_unmap_page(L4_FpageLog2( vaddr, bits ) + rwx);
		    if (!do_flush)
			return;
		}
	    }
#endif

	    lock();
	    flush |= do_flush;
	    unlock();
	    
	    if( count == cache_size )
		commit();
	    lock();
	    fpages[count++] = L4_FpageLog2( kaddr, bits ) + rwx;
	    unlock();
	}

    unmap_cache_t()
	{ count = 0; flush = false; }
    
    word_t commit()
	{
	    lock();
	    
	    if( !this->count )
	    {
		unlock();
		return 0;
	    }

	    if( flush )
		L4_FlushFpages( this->count, this->fpages );
	    else
		L4_UnmapFpages( this->count, this->fpages );
	    
	    word_t ret = 0;
	    for (word_t n=0; n<count;n++)
		ret |= L4_Rights(this->fpages[n]);
	
    	    this->count = 0;
	    this->flush = false;
	    unlock();
	    return ret;
	}

};

extern unmap_cache_t unmap_cache;




#endif /* !__L4KA__MEMORY_H__ */

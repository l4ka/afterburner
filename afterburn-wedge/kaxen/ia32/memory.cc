/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/kaxen/ia32/memory.cc
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
 ********************************************************************/

#include INC_ARCH(page.h)
#include INC_ARCH(cpu.h)

#include INC_WEDGE(memory.h)
#include INC_WEDGE(xen_hypervisor.h)
#include <console.h>
#include <debug.h>
#include INC_WEDGE(vcpulocal.h)

#include <string.h>
#include <burn_counters.h>

DECLARE_BURN_COUNTER(ptab_virgin_user);
DECLARE_BURN_COUNTER(pdir_pin);
DECLARE_BURN_COUNTER(pdir_virgin);

// Batching page table changes effects two types of visibility: the TLB, and
// in the page table itself.  To achieve proper TLB visibility, we can batch
// and then commit at a TLB invalidation (unless the old entry is already
// invalid).  Unfortunately, for page table visibility, we have to commit
// immediately.  If we had a perfect hook for all page table reads, then
// we could collaborate with that hook to support delayed commits.  But
// we don't have such a hook.

void SECTION(".text.pte")
xen_memory_t::change_pgent( pgent_t *old_pgent, pgent_t new_pgent, bool leaf )
    // All changes to page tables happen via this function.  Thus every pgent,
    // in a page table, will have a machine address; there is no chance 
    // that a live pgent has a physical address.
{
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());

    bool good = true;
    bool device = false;
    word_t new_paddr = 0;
    mach_page_t *new_mpage = 0;
    mach_page_t &mpage = p2mpage(guest_v2p(word_t(old_pgent)));
    word_t pgent_maddr = mpage.get_address() | (word_t(old_pgent) & ~PAGE_MASK);
    word_t pdir_entry = (word_t(old_pgent) & ~PAGE_MASK)/sizeof(pgent_t);

    // The page containing old_pgent is a page table by definition.  It is
    // possible that the new pgent is pointing back at the page table.
    // Thus make sure that we convert the pgent to read-only if necessary.
    mpage.set_pgtable();

#if !defined(CONFIG_KAXEN_GLOBAL_PAGES)
    ASSERT( !new_pgent.is_valid() || !new_pgent.is_global() );
#endif

    if( !leaf ) {
	if( pdir_entry < guest_kernel_pdent() )
	    INC_BURN_COUNTER(pgd_set_user);
	else
	    INC_BURN_COUNTER(pgd_set_kernel);
	if( new_pgent.get_raw() == 0 )
	    INC_BURN_COUNTER(pgd_set_zero);
	if( mapping_base_maddr == mpage.get_address() )
	    INC_BURN_COUNTER(pgd_set_current);
    }

#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    mach_page_t *old_mpage = 0;
    if( !leaf && old_pgent->is_xen_ptab_valid() ) {
	// The old pgent is pointing at a page table.  Decrement the link count.
	INC_BURN_COUNTER(unpin_attempt);
	old_mpage = &p2mpage( m2p(old_pgent->get_address()) );
	ASSERT( old_mpage->is_pgtable() );
	if( old_mpage->is_pinned() )
	    old_mpage->link_dec();
    }
#endif

    if( new_pgent.is_valid() )
    {
	ASSERT( leaf || !new_pgent.is_superpage() );

	// Always insert a PTE entry that has a Xen machine address, to avoid
       	// scanning the entire page table at a later time.
	if( new_pgent.is_xen_machine() ) {
	    new_paddr = m2p( new_pgent.get_address() );
	    new_mpage = &p2mpage( new_paddr );
	}
	else if( EXPECT_FALSE(is_device_memory(new_pgent.get_address())) ) {
	    if( !leaf )
		PANIC("Attempt to use a device address in a page directory entry.");
#if defined(CONFIG_DEVICE_APIC)
	    word_t ioapic_id;
	    if (acpi.is_lapic_addr(new_pgent.get_address()))
	    {
		pgent_t lapic_pgent = get_pgent((word_t) get_cpu().lapic);
		
		if (debug_apic)
		    printf( "Guest tries to map LAPIC @ " << (void *) new_pgent.get_address()
			<< ", mapping softLAPIC @ " << (void *) get_cpu().get_lapic() 
			<< " / " << (void *) lapic_pgent.get_address()
			<< " \n");

		new_paddr = lapic_pgent.get_address();
		new_pgent.set_address( lapic_pgent.get_address() );
		new_pgent.set_xen_machine();
		
	    }
	    else if (acpi.is_ioapic_addr(new_pgent.get_address(), &ioapic_id))
	    {
		word_t ioapic_addr = 0;
		intlogic_t &intlogic = get_intlogic();

		for (word_t idx = 0; idx < CONFIG_MAX_IOAPICS; idx++)
		    if ((intlogic.ioapic[idx].get_id()) == ioapic_id)
			ioapic_addr = (word_t) &intlogic.ioapic[idx];
	    
		if (ioapic_addr == 0)
		{
		    printf( "BUG: Access to nonexistent IOAPIC (id " << ioapic_id << ")\n"); 
		    panic();
		}
		pgent_t ioapic_pgent = get_pgent(ioapic_addr);
		    
		if (debug_apic)
		    printf( "Guest tries to map IOAPIC " << ioapic_id << " @ " << (void *) new_pgent.get_address()
			<< ", mapping softIOAPIC @ " << (void *) ioapic_addr
			<< " / " << (void *) ioapic_pgent.get_address()
			<< "\n");
	
		new_paddr = ioapic_pgent.get_address();
		new_pgent.set_address( ioapic_pgent.get_address() );
		new_pgent.set_xen_machine();
	    }
	    else 
#endif

	    device = true;
	}
	else
	{
	    new_paddr = new_pgent.get_address();
	    new_mpage = &p2mpage( new_pgent.get_address() );
	    new_pgent.set_address( new_mpage->get_address() );
	    new_pgent.set_xen_machine();
	}

	if( !leaf ) {
	    new_mpage->set_pgtable();
#if defined(CONFIG_KAXEN_UNLINK_AGGRESSIVE)
	    // Set a back-link from the ptab to the pdir entry.
	    new_mpage->set_pdir_entry( pdir_entry );
#endif
	}
	else if( leaf && !device && new_mpage->is_pinned() ) {
	    // Since the target page is a pinned page table, we must
	    // have a read-only mapping.  We do read-only even if our current
	    // page table is unpinned, because there is no benefit of a
	    // read-write mapping (if the current page table is unpinned, then
	    // the new pgentry is inactive anyway).
	    new_pgent.set_read_only();
	    new_pgent.set_xen_special();
	}

	if( !leaf && !new_mpage->is_pinned() )
	{
#if defined(CONFIG_KAXEN_UNLINK_HEURISTICS)
	    if( pdir_entry < guest_kernel_pdent() ) {
		// We're inserting a pdir entry to an unpinned page table.
		// We thus insert a broken entry, so that we don't have to pin
		// the page table.
    		new_pgent.set_xen_unlinked();
	    }
	    else 
#endif
	    {
		new_mpage->set_pinned();
		word_t primary_vaddr = guest_p2v(new_paddr);
#if 0
		if( pdir_entry == pgent_t::get_pdir_idx(primary_vaddr) ) {
		    // We are installing a ptab that contains a self reference.
		    // But perhaps we don't have a virtual address for the
		    // incoming page table.
#if 0
		    pgent_t *primary_pgent2 = &((pgent_t *)primary_vaddr)[ pgent_t::get_ptab_idx(primary_vaddr) ];
		    primary_pgent2->set_read_only();
		    primary_pgent2->set_xen_special();
		    primary_pgent2->set_address( new_mpage->get_address() );
		    primary_pgent2->set_xen_machine();
#endif
		} 
		else
#endif
		{
		    pgent_t primary_pgent = get_pgent(primary_vaddr);
		    primary_pgent.set_read_only();
		    primary_pgent.set_xen_special();
		    good &= mmop_queue.ptab_update(
			    get_pgent_maddr(primary_vaddr), 
			    primary_pgent.get_raw() );
//    		    good &= mmop_queue.invlpg( primary_vaddr );
		}

#if 0
		if( get_pdent(new_paddr).is_valid() ) {
		    pgent_t secondary_pgent = get_pgent( new_paddr );
		    if( secondary_pgent.is_valid() && secondary_pgent.is_xen_machine() && (secondary_pgent.get_address() == new_pgent.get_address()) )
		    {
			ASSERT( pgent_t::get_pdir_idx(new_paddr) != pdir_entry );
			secondary_pgent.set_read_only();
			secondary_pgent.set_xen_special();
			good &= mmop_queue.ptab_update(
				get_pgent_maddr(new_paddr), 
				secondary_pgent.get_raw() );
//			good &= mmop_queue.invlpg( new_paddr );
		    }
		}
#endif

		good &= mmop_queue.pin_table(new_mpage->get_address(), 0);
	    }
	}
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
	if( !leaf && mpage.is_pinned() && new_mpage->is_pinned() )
	    // The link in Xen is only created if both the page dir
	    // and the page table are pinned.
	    new_mpage->link_inc();
#endif
    }

#if defined(CONFIG_KAXEN_UNLINK_AGGRESSIVE)
    if( leaf && mpage.is_pinned() && last_pdir_vaddr && (last_pdir_vaddr[mpage.get_pdir_entry()].get_address() == mpage.get_address()) )
    {
	// Unpin the page table.
	if( unlink_pdent_heuristic(last_pdir_vaddr, mpage, false) )
	    INC_BURN_COUNTER(pte_set_unpin_prior);
    }
    if( leaf && mpage.is_pinned() && (get_pdir()[mpage.get_pdir_entry()].get_address() == mpage.get_address()) )
    {
	// Unpin the page table.
	if( unlink_pdent_heuristic(get_guest_pdir(), mpage, true) )
	    INC_BURN_COUNTER(pte_set_unpin);
    }
#endif

#if defined(CONFIG_KAXEN_PGD_COUNTING)
    if( !leaf && (pdir_entry < guest_kernel_pdent()) ) {
	if( new_pgent.is_xen_ptab_valid() && !old_pgent->is_xen_ptab_valid() )
	    mpage.inc_pgds();
	else if( !new_pgent.is_xen_ptab_valid() && old_pgent->is_xen_ptab_valid() )
	    mpage.dec_pgds();
    }
#endif

#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    // Don't use writable page tables for page fault handling, because we need
    // to iret to a valid address.  With writable page tables, the iret
    // goes to an invalid address, since the page table is detached.
    // A pinned page table must be linked to a page directory in order to use
    // Xen's writable page tables.
    if( !mpage.is_pinned() || (leaf && (get_cpu().cr2 == 0) && mpage.is_single_link()) )
#else
    if( !mpage.is_pinned() )
#endif
    {
	// The page table is writable.  We don't need hypercalls.
	ON_BURN_COUNTER(cycles_t start = get_cycles());
	*old_pgent = new_pgent;
	ON_BURN_COUNTER(if( (get_cycles() - start) > 500 ) INC_BURN_COUNTER(pte_set_pgfault));
	INC_BURN_COUNTER(pte_set_fast);
    }
    else
    {
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
	ASSERT( leaf || !new_pgent.is_valid() || new_mpage->is_pinned() );
	if( get_cpu().cr2 != 0 ) {
	    if( mpage.is_single_link() && leaf )
		INC_BURN_COUNTER(cr2_collision);
	    else
		INC_BURN_COUNTER(cr2_collision_complex);
	}
	else if( leaf )
	    INC_BURN_COUNTER( pte_set_slow_multi );
	else
	    INC_BURN_COUNTER( pte_set_slow_pgd );
#endif
	// The page table is pinned.  Use a hypercall.
	good &= mmop_queue.ptab_update( pgent_maddr, 
		new_pgent.get_raw() );
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
	if( !leaf || (new_pgent.get_raw() != 0) )
#else
	if( new_pgent.get_raw() != 0 )
#endif
	    good &= mmop_queue.commit();
        if( !good )
    	    PANIC( "Failed to update a page entry, old %p, new %p"
		   ", raw 0x%lx, leaf %i",
    		   old_pgent->is_valid() ? 
		     m2p(old_pgent->get_address()) : old_pgent->get_raw(),
    		   new_pgent.is_valid() ? 
			m2p(new_pgent.get_address()) : new_pgent.get_raw(),
		   new_pgent.get_raw(), leaf );
	INC_BURN_COUNTER(pte_set_slow);
    }

#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    if( old_mpage && old_mpage->is_pinned() && old_mpage->is_unlinked() )
    {
	INC_BURN_COUNTER(unpin_set_pte);
	old_mpage->set_normal();

	word_t mpage_vaddr = guest_p2v(m2p(old_mpage->get_address()));
	pgent_t mpage_pgent = get_pgent( mpage_vaddr );
	ASSERT( mpage_pgent.get_address() == old_mpage->get_address() );
	ASSERT( mpage_pgent.is_xen_special() );
	ASSERT( mpage_pgent.is_valid() );
	ASSERT( new_pgent.get_address() != old_mpage->get_address() );
	mpage_pgent.clear_xen_special();
	mpage_pgent.set_writable();

	good &= mmop_queue.unpin_table( old_mpage->get_address() );
	good &= mmop_queue.ptab_update( get_pgent_maddr(mpage_vaddr), 
					mpage_pgent.get_raw() );
	good &= mmop_queue.invlpg( mpage_vaddr );
	good &= mmop_queue.commit();
	if( !good )
	    PANIC( "Failed to unpin an unlinked page table." );
    }
#endif

    ADD_PERF_COUNTER(pte_set_cycles, get_cycles()-cycles);
}


void xen_memory_t::manage_page_dir( word_t new_pdir_phys )
{
    bool good = true;

    mach_page_t &mpage = p2mpage( new_pdir_phys );
    ASSERT( !mpage.is_pinned() );

    word_t new_pdir_vaddr = guest_p2v( new_pdir_phys );
    pgent_t *new_pdir = (pgent_t *)new_pdir_vaddr;

    // We have a virgin page directory.
    // Assume that the guest OS copied the page dir entries from
    // one page dir to another.
    INC_BURN_COUNTER(pdir_virgin);
    word_t pd = 0;
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    // Now that we pin the page dir, Xen will create links between the
    // page dir and the pinned page tables.  So we must follow suit.
    word_t user_pd = guest_kernel_pdent();
    for( ; pd < user_pd; pd++ )
    {
	if( !new_pdir[pd].is_valid() )
	    continue;
	INC_BURN_COUNTER(ptab_virgin_user);
	ASSERT( new_pdir[pd].is_xen_machine() );
	mach_page_t ptab_mpage = p2mpage( m2p(new_pdir[pd].get_address()) );
	ASSERT( ptab_mpage.is_pgtable() );
	ASSERT( ptab_mpage.is_pinned() );
	ptab_mpage.link_inc();
#if defined(CONFIG_KAXEN_PGD_COUNTING)
	mpage.inc_pgds();
#endif
    }
    for( ; pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd++ )
    {
	if( !new_pdir[pd].is_valid() )
	    continue;
	ASSERT( new_pdir[pd].is_xen_machine() );
	mach_page_t ptab_mpage = p2mpage( m2p(new_pdir[pd].get_address()) );
	ASSERT( ptab_mpage.is_pgtable() );
	ASSERT( ptab_mpage.is_pinned() );
	ptab_mpage.link_inc();
    }
#endif

    mpage.set_pgtable();
    INC_BURN_COUNTER(pdir_pin);

    // Copy the page directory entries of the wedge into the new page table.
    for( pd = CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd < pdir_end_entry; pd++ )
    {
	new_pdir[pd] = get_pdir()[pd];
    }

    // Recursively map the page directory.
    new_pdir[ pgent_t::get_pdir_idx(word_t(pgtab_region)) ].set_address(mpage.get_address());

    // Ensure that the new page directory is read-only mapped at its
    // virtual address in the guest's space.
    pgent_t new_pdir_pgent = get_pgent( new_pdir_vaddr );
    if( new_pdir_pgent.is_writable() ) {
	new_pdir_pgent.set_read_only();
	new_pdir_pgent.set_xen_special();
	good &= mmop_queue.ptab_update( get_pgent_maddr(new_pdir_vaddr), 
		new_pdir_pgent.get_raw() );
#if defined(CONFIG_KAXEN_GLOBAL_PAGES)
	good &= mmop_queue.invlpg( new_pdir_vaddr );
#else
	// Since we are switching the page directory, we don't have
	// to flush the old TLB entry.
#endif
    }

    // Pin the page directory.
    mpage.set_pinned();
    good &= mmop_queue.pin_table( mpage.get_address(), 1 );
}


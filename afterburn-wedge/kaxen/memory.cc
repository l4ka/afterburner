/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/kaxen/memory.cc
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

#if defined(CONFIG_DEVICE_APIC)
#include <device/acpi.h>
#include <device/apic.h>
#endif

static const bool debug_unpin=0;

DECLARE_BURN_COUNTER(xen_upcall_pending);
DECLARE_BURN_COUNTER(unpin_page_fault);
DECLARE_BURN_COUNTER(unpin_set_pte);
DECLARE_BURN_COUNTER(unpin_attempt);
DECLARE_BURN_COUNTER(ptab_delayed_link);
DECLARE_PERF_COUNTER(ptab_delayed_link_cycles);
DECLARE_BURN_COUNTER(ptab_delayed_pin);
DECLARE_BURN_COUNTER(pgd_set_kernel);
DECLARE_BURN_COUNTER(pgd_set_user);
DECLARE_BURN_COUNTER(pgd_set_zero);
DECLARE_BURN_COUNTER(pgd_set_current);
DECLARE_BURN_COUNTER(pte_set_pgfault);
DECLARE_BURN_COUNTER(pte_set_fast);
DECLARE_BURN_COUNTER(pte_set_slow);
DECLARE_BURN_COUNTER(pte_set_slow_multi);
DECLARE_BURN_COUNTER(pte_set_slow_pgd);
DECLARE_BURN_COUNTER(pte_set_unpin);
DECLARE_BURN_COUNTER(pte_set_unpin_prior);
DECLARE_PERF_COUNTER(pte_set_unpin_cycles);
DECLARE_PERF_COUNTER(pte_set_cycles);
DECLARE_BURN_COUNTER(cr2_collision);
DECLARE_BURN_COUNTER(cr2_collision_complex);
DECLARE_BURN_COUNTER(tlb_flush);
DECLARE_BURN_COUNTER(pdir_switch);
DECLARE_PERF_COUNTER(pdir_switch_cycles);
DECLARE_BURN_COUNTER(pdir_switch_unpin);

static pgent_t * last_pdir_vaddr = 0;

/* VU: Xen 2.0 and 3.0 have different hypercall interfaces for memory
 * operations.  We hide the differences via multicalls (besides
 * avoiding crazy overheads) */

#ifdef CONFIG_XEN_2_0
bool xen_mmop_queue_t::commit()
{
    unsigned success_cnt;
    word_t commits = this->count;
    this->count = 0;
    if( XEN_mmu_update(req, commits, &success_cnt) || success_cnt != commits )
	return false;
    xen_do_callbacks();
    return true;
}

bool xen_mmop_queue_t::add( word_t type, word_t ptr, word_t val, bool sync )
{
    req[count].ptr = type | (ptr & ~word_t(3));
    req[count].val = val;
    count++;
    if( count == queue_len || sync )
	return commit();
    return true;
}

bool xen_mmop_queue_t::add_ext( word_t type, word_t ptr, bool sync )
{
    return add( MMU_EXTENDED_COMMAND, ptr, type, sync );
}

#elif CONFIG_XEN_3_0

bool xen_mmop_queue_t::commit()
{
    //printf( "commit cnt=%lu, ext=%lu, mc=%lu\n", count, ext_count, mc_count );
    
    if (count == 0 && ext_count == 0)
	return true;

    word_t commits = mc_count;
    //printf("%u\n", commits);
    count = ext_count = mc_count = 0;

    // everything is prepared, just commit the syscalls...
    if (XEN_multicall(multicall, commits))
	return false;

    // the multicall succeeded, but maybe the individual calls
    // failed?  iterate...
    for (unsigned i = 0; i < commits; i++)
	if (multicall[i].result != 0)
	    return false;
    
    return true;
}

bool xen_mmop_queue_t::add( word_t type, word_t ptr, word_t val, bool sync )
{
    if (mc_count == 0 || multicall[mc_count - 1].op != __HYPERVISOR_mmu_update)
    {
	multicall[mc_count].op = __HYPERVISOR_mmu_update;
	multicall[mc_count].args[0] = (word_t)&req[count];
	multicall[mc_count].args[1] = 0; // count
	multicall[mc_count].args[2] = (word_t)&multicall_success[mc_count];
	multicall[mc_count].args[3] = DOMID_SELF;
	mc_count++;
    }

    req[count].ptr = type | (ptr & ~word_t(3));
    req[count].val = val;
    count++;
    multicall[mc_count - 1].args[1]++;

    if( count == queue_len || sync )
	return commit();
    return true;
}

bool xen_mmop_queue_t::add_ext( word_t type, word_t ptr, bool sync )
{
    if (mc_count == 0 || multicall[mc_count - 1].op != __HYPERVISOR_mmuext_op)
    {
	multicall[mc_count].op = __HYPERVISOR_mmuext_op;
	multicall[mc_count].args[0] = (word_t)&ext_req[ext_count];
	multicall[mc_count].args[1] = 0; // count
	multicall[mc_count].args[2] = (word_t)&multicall_success[mc_count];
	multicall[mc_count].args[3] = DOMID_SELF;
	mc_count++;
    }

    ext_req[ext_count].cmd = type;
    ext_req[ext_count].arg1.mfn = ptr;
    ext_req[ext_count].arg1.linear_addr = ptr;
    ext_count++;
    multicall[mc_count - 1].args[1]++;

    if( ext_count == ext_queue_len || sync )
	return commit();
    return true;
}
#else
# error Xen >3.0 probably broke that again...
#endif


#if defined(CONFIG_KAXEN_UNLINK_AGGRESSIVE)
// XXX DONTCARE UNKNOWN
bool xen_memory_t::unlink_pdent_heuristic( pgent_t *pdir, mach_page_t &mpage, bool cr2 )
{
    if( mpage.get_pdir_entry() >= guest_kernel_pdent() )
	return false;
    if( cr2 && (get_cpu().cr2 != 0) )
//	    (pgent_t::get_pdir_idx(get_cpu().cr2) == mpage.get_pdir_entry()) )
    {
       	// We're servicing a page fault, don't unlink.
	INC_BURN_COUNTER(cr2_collision);
	return false;
    }

    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    bool good = true;
    mpage.set_unpinned();

    // Unlink the page dir entry.
    pgent_t pdent = pdir[ mpage.get_pdir_entry() ];
    ASSERT( pdent.is_valid() );
    ASSERT( pdent.get_address() == mpage.get_address() );
    pdent.set_xen_unlinked();
    word_t pdent_maddr = p2m(guest_v2p(word_t(pdir))) + mpage.get_pdir_entry()*sizeof(pgent_t);
    good &= mmop_queue.ptab_update( pdent_maddr, pdent.get_raw());

    // Unpin the page table.
    good &= mmop_queue.unpin_table( mpage.get_address() );

    // Enable write permissions for the page table's mapping.
    word_t mpage_vaddr = guest_p2v( m2p(mpage.get_address()) );
    pgent_t pgent = get_pgent( mpage_vaddr );
    ASSERT( pgent.get_address() == mpage.get_address() );
    ASSERT( pgent.is_xen_special() );
    pgent.set_writable();
    pgent.clear_xen_special();
    good &= mmop_queue.ptab_update( get_pgent_maddr(mpage_vaddr),
	    pgent.get_raw() );
    good &= mmop_queue.invlpg( mpage_vaddr );

    good &= mmop_queue.commit();
    if( !good )
	PANIC( "Unable to unlink and unpin a page table." );

    ADD_PERF_COUNTER(pte_set_unpin_cycles, get_cycles()-cycles);
    return true;
}
#endif

#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
// XXX DONTCARE UNKNOWN
void xen_memory_t::unmanage_page_dir( word_t pdir_vaddr )
{
    word_t i = 0;
    pgent_t *pdir = (pgent_t *)(pdir_vaddr & PAGE_MASK);
#if 0
    for( ; i < guest_kernel_pdent(); i++ )
    {
	if( !pdir[i].is_xen_ptab_valid() )
	    continue;
	mach_page_t &mpage = p2mpage( m2p(pdir[i].get_address()) );
	if( mpage.is_pinned() )
	    mpage.link_dec();
    }
#endif
#if 1
    for( ; i < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; i++ )
    {
	if( !pdir[i].is_xen_ptab_valid() )
	    continue;
	mach_page_t &mpage = p2mpage( m2p(pdir[i].get_address()) );
	if( mpage.is_pinned() )
	    mpage.link_dec();
    }
#endif
}
#endif

void xen_memory_t::switch_page_dir( word_t new_pdir_phys, word_t old_pdir_phys )
{
    bool good = true;

    if( new_pdir_phys == old_pdir_phys ) {
	INC_BURN_COUNTER(tlb_flush);
	good = mmop_queue.tlb_flush( true );
	if( !good )
	    PANIC( "TLB flush failure.");
	return;
    }

    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    INC_BURN_COUNTER(pdir_switch);
    last_pdir_vaddr = (pgent_t *)guest_p2v(old_pdir_phys);

    mach_page_t &mpage = p2mpage( new_pdir_phys );
    if( !mpage.is_pinned() )
	manage_page_dir( new_pdir_phys );

    // Switch the page directory.
    this->mapping_base_maddr = mpage.get_address();
    good &= mmop_queue.set_baseptr( mpage.get_address(), false );

#if defined(CONFIG_KAXEN_PGD_COUNTING)
    mach_page_t &old_mpage = p2mpage(old_pdir_phys);
    if( old_mpage.is_empty_pgds() ) {
	unpin_page( old_mpage, old_pdir_phys, false );
	INC_BURN_COUNTER(pdir_switch_unpin);
    }
#endif

    good &= mmop_queue.commit();
    if( !good )
	PANIC( "Failed to switch the page directory." );

    ASSERT( get_pgent(guest_p2v(new_pdir_phys)).get_address() == get_pgent(word_t(mapping_base_region)).get_address() );
#ifdef CONFIG_ARCH_IA32
    ASSERT( get_pdent(word_t(pgtab_region)).get_address() == mpage.get_address() );
#endif
    // TODO amd64 asserts

    ADD_PERF_COUNTER(pdir_switch_cycles, get_cycles()-cycles);
}


void xen_memory_t::unpin_page( mach_page_t &mpage, word_t paddr, bool commit )
{
    ASSERT( mpage.is_pinned() );
    
    // NOTE: we assume that the vaddr is valid, and thus resolvable.
    word_t vaddr = guest_p2v( paddr );
    pgent_t pgent = get_pgent( vaddr );
    ASSERT( pgent.get_address() == mpage.get_address() );

    if( debug_unpin )
	printf( "Unpinning a page table, vaddr %p, maddr %p, paddr %p\n",
		vaddr, mpage.get_address(), paddr );

#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    ASSERT( mpage.is_unlinked() );
    unmanage_page_dir( vaddr );
#endif
    mpage.set_normal();
    pgent.clear_xen_special();
    pgent.set_writable();

    bool good = mmop_queue.unpin_table( mpage.get_address() );
    good &= mmop_queue.ptab_update( get_pgent_maddr(vaddr), pgent.get_raw() );
    good &= mmop_queue.invlpg( vaddr );
    if( commit ) {
	good &= mmop_queue.commit();
	if( !good )
	    PANIC( "Attempt to unpin a page that is still in use." );
    }
}

bool SECTION(".text.pte")
xen_memory_t::resolve_page_fault( xen_frame_t *frame )
{
#ifdef CONFIG_ARCH_AMD64
    PANIC("page fault at %p accessing %p", frame->iret.ip, frame->info.fault_vaddr);
    // TODO amd64
#else
    pgfault_err_t err;
    err.x.raw = frame->info.error_code;

    ASSERT( !err.is_reserved_fault(), frame );
    ASSERT( err.is_user_fault() == (cpu_t::get_segment_privilege(frame->iret.cs) == 3) );

    if( err.is_valid() && err.is_write_fault() && !err.is_user_fault() )
    {
	pgent_t pgent = get_pgent( frame->info.fault_vaddr );
	if( pgent.is_kernel() && pgent.is_xen_special() )
	{
	    ASSERT( frame->info.fault_vaddr < CONFIG_WEDGE_VIRT, frame );
	    // Is it a page table?  Then we need to unpin and release.
	    mach_page_t &mpage = p2mpage( m2p(pgent.get_address()) );
	    if( mpage.is_pgtable() && mpage.is_pinned() )
	    {
		INC_BURN_COUNTER(unpin_page_fault);
		if( debug_unpin )
		    printf( "Unpinning a page table, vaddr %p, maadr %p"
			    ", paddr %p\n",
			    frame->info.fault_vaddr,
			    pgent.get_address(),
			    m2p(pgent.get_address()) );

#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
		ASSERT( mpage.is_unlinked(), frame );
		unmanage_page_dir( frame->info.fault_vaddr );
#endif

		mpage.set_normal();
		pgent.clear_xen_special();
		pgent.set_writable();

		ASSERT( pgent.get_address() == mpage.get_address(), frame );
		bool good = mmop_queue.unpin_table( mpage.get_address() );
		good &= mmop_queue.ptab_update( get_pgent_maddr(frame->info.fault_vaddr),
						pgent.get_raw() );
//		good &= mmop_queue.invlpg( frame->info.fault_vaddr );
		good &= mmop_queue.commit();
		if( !good )
		    PANIC( "Attempt to unpin a page that is still in use.", 
			    frame );
		return true;
	    }
	}
    }

#if defined(CONFIG_KAXEN_UNLINK_HEURISTICS)
    else if( !err.is_valid() )
    {
	// Do we have an unlinked page table?
	pgent_t pdent = get_pdent( frame->info.fault_vaddr );
	if( pdent.is_xen_unlinked() ) {
	    relink_ptab( frame->info.fault_vaddr );
	    return true;
	}
    }
#endif
#endif

    return false;
}

#if defined(CONFIG_KAXEN_UNLINK_HEURISTICS)
// XXX DONTCARE UNKNOWN
void xen_memory_t::relink_ptab( word_t fault_vaddr )
{
    INC_BURN_COUNTER(ptab_delayed_link);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    bool good = true;

    pgent_t pdent = get_pdent( fault_vaddr );
    ASSERT( pdent.is_xen_unlinked() );

    // Pin the unlinked page table.
    ASSERT( pdent.is_xen_machine() );
    word_t paddr = m2p( pdent.get_address() );
    mach_page_t &mpage = p2mpage( paddr );
    ASSERT( mpage.is_pgtable() );
    if( !mpage.is_pinned() )
    {
	INC_BURN_COUNTER(ptab_delayed_pin);
	word_t vaddr = guest_p2v(paddr);
	pgent_t pgent = get_pgent( vaddr );
	pgent.set_read_only(); pgent.set_xen_special();
	mpage.set_pinned();
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
	// Create a link between the pinned page dir, and this newly
	// pinned page table.
	mpage.link_inc();
#endif
	good &= mmop_queue.ptab_update( get_pgent_maddr(vaddr), pgent.get_raw() );
//	good &= mmop_queue.invlpg( vaddr );
	good &= mmop_queue.pin_table( mpage.get_address(), 0 );
    }

    // Link the page table.
    pdent.set_xen_linked();
    good &= mmop_queue.ptab_update( get_pdent_maddr(fault_vaddr), 
	    pdent.get_raw() );
    good &= mmop_queue.commit();
    if( !good )
	PANIC( "Unable to perform a delayed page table link." );
    ADD_PERF_COUNTER(ptab_delayed_link_cycles, get_cycles()-cycles);
}
#endif


void xen_memory_t::flush_vaddr( word_t vaddr )
{
    bool good = mmop_queue.invlpg( vaddr & PAGE_MASK, true );
    if( !good )
	PANIC( "Xen invlpg failed for virtual address %p", vaddr );
}


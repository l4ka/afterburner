/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/kaxen/amd64/memory.cc
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

void xen_memory_t::manage_page_dir( word_t new_pdir_phys )
{
    UNIMPLEMENTED();
    // TODO
#if 0
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
#endif
}


/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/memory_init.cc
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

#include <memory.h>

static const bool debug_boot_pdir = false;
static const bool debug_remap_boot_region = false;
static const bool debug_alloc_boot_ptab = false;
static const bool debug_alloc_remaining = false;
static const bool debug_alloc_boot_region = false;
static const bool debug_map_device = false;
static const bool debug_contiguous = true;

xen_memory_t xen_memory;
xen_mmop_queue_t mmop_queue;

/* TODO:
 * - Reclaim pages from the ELF headers.
 * - Reclaim pages from unused boot structures provided by Xen, including
 *   the initial stack.
 * - Compress the virtual region of the wedge to minimize the number
 *   of page directory entries to copy into a new page directory.
 * - Don't allocate pages for the low-memory regions that belong to devices.
 */

/* xen_start_info.mfn_list: The mfn_list is an array of page frame 
 * numbers for each machine frame allocated to this VM.  Since Xen 
 * doesn't actually virtualize the hardware, these page frames won't 
 * disappear, unless the memory ballooning module is enabled.
 *
 * We maintain a mapping of guest-to-machine frames, using the
 * xen_p2m_region.  This region is physically allocated at boot, with a 
 * sufficient physical backing for an array that can represent the number
 * of pages assigned to the virtual machine, but using a fixed 4MB virtual
 * region.  Each array entry is 4-bytes, and is a mach_page_t.  It stores
 * the machine page number in the upper 20 bits, and information about the
 * page in the lower 12 bits.  The page info expresses the page
 * type (whether normal, page table, or descriptor), and whether it has been
 * pinned via a Xen pinning system call.  The function p2m() maps a 
 * guest's physical page address to a machine page address.  
 * And p2mpage() maps a guest's physical page address to a mach_page_t.
 *
 * Xen exports a table to perform the machine to physical mapping.  It
 * is called machine_to_phys_mapping.  The function m2p() maps a machine
 * page address to a guest's physical page address.
 */

void xen_memory_t::validate_boot_pdir()
    /* Future proof ourselves, and verify that the initial page layout
     * is as expected (since the Xen protocol is arbitrary, naive, and likely
     * to change).  The vaddr of the pgdir is in the xen_start_info
     * structure.  Since the pgdir entries point at physical addresses for 
     * the leaf tables, we rely on a pre-arranged protocol for locating the
     * leaf tables in the vaddr space: Each leaf table is located successively
     * after the pdir in the vaddr space.  Each machine page pointed to by 
     * the page tables entries is located in the xen_start_info.mfn_list, 
     * ordered in ascending order by virtual address.  After we 
     * change any mappings, the ordering of the mfn_list no longer strictly 
     * matches the page tables.
     *
     * VU: we make the assumption that special pages are at the
     * beginning.  Otherwise we need a bitmap!!!
     */
{
    pgent_t *pdir = get_boot_pdir();
    word_t *mfn_list = (word_t *)xen_start_info.mfn_list;
    word_t i, j, found, pg_cnt, vaddr, alloc_start;

    pg_cnt = 0;  // Number of valid leaf page table entries found.
    found = 1;   // Number of page tables and directories found.
    alloc_start = 0;

    for( i = 0; i < HYPERVISOR_VIRT_START/PAGEDIR_SIZE; i++ )
    {
	vaddr = i*PAGEDIR_SIZE;
	if( !pdir[i].is_valid() )
	    continue;

	if( pdir[i].is_superpage() ) {
	    if( debug_boot_pdir ) {
		dump_pgent( 0, vaddr, pdir[i], SUPERPAGE_SIZE );
		continue;
	    }
	    PANIC( "Unexpected superpage mapping." );
	}
	if( debug_boot_pdir )
	    dump_pgent( 0, vaddr, pdir[i], PAGE_SIZE );

	if( boot_pdir_start_entry == word_t(-1) )
	    boot_pdir_start_entry = i;

	pgent_t *ptab = get_boot_ptab( i );
	found++;

	for( j = 0; j < PAGE_SIZE/sizeof(pgent_t); j++ )
	{
	    vaddr = i*PAGEDIR_SIZE + j*PAGE_SIZE;
	    if( !ptab[j].is_valid() )
		continue;
	    if( debug_boot_pdir ) {
		dump_pgent( 1, vaddr, ptab[j], PAGE_SIZE );
		printf( "  list=" << (void*)mfn_list[pg_cnt+alloc_start] << '\n';
	    }
    
	    if( vaddr >= (CONFIG_WEDGE_VIRT + CONFIG_WEDGE_WINDOW) ) {
		if (mfn_list[pg_cnt + alloc_start] != (ptab[j].get_address() >> PAGE_BITS))
	    	    PANIC( "Unexpected ordering in the MFN list." );
		pg_cnt++;
	    }
	    else {
		// In Xen 3, the initial virtual address space
		// is 4MB aligned, and thus the page tables may have PTEs
		// before our wedge code.  The wedge uses virtual addresses
		// in front of its code section for remapping Xen data
		// structures such as the shared page.  Thus the page
		// tables may not match the mfn_list, since we may have
		// already altered these page table entries.
		if (debug_boot_pdir) 
		    printf( "found special page at " << (void*)vaddr << '\n';
#ifndef CONFIG_XEN_2_0
		alloc_start++;
#endif
	    }
	}
    }

    boot_mfn_list_allocated = pg_cnt;
    boot_mfn_list_start = alloc_start;

    ASSERT( found == xen_start_info.nr_pt_frames );
    ASSERT( boot_mfn_list_allocated );
    ASSERT( boot_mfn_list_allocated > boot_mfn_list_start );
}

void xen_memory_t::map_boot_pdir()
{
    // Determine the machine address of the boot pdir, based on Xen's
    // boot protocol.
    word_t pdir_vaddr = (word_t)get_boot_pdir();
    pgent_t *ptab = get_boot_ptab( pgent_t::get_pdir_idx(pdir_vaddr) );
    pgent_t &pgent = ptab[ pgent_t::get_ptab_idx(pdir_vaddr) ];
    this->pdir_maddr = this->boot_pdir_maddr = pgent.get_address();

    if( get_boot_pdir()[pgent_t::get_pdir_idx(word_t(pgtab_region))].is_valid() )
    {
	printf( (void*)get_boot_pdir()[pgent_t::get_pdir_idx(word_t(pgtab_region))].get_raw();
	PANIC( "A mapping already exists for the page table region." );
    }

    // Recursively map the pdir, so that all leaf page tables are accessible
    // in the pgtab_region.
    if( !mmop_queue.ptab_update( get_pdent_maddr(word_t(pgtab_region)), 
				 pgent.get_raw(), true ) )
	PANIC( "Unable to recursively map the boot page directory." );
}

void xen_memory_t::globalize_wedge()
{
#if defined(CONFIG_KAXEN_GLOBAL_PAGES)
    pgent_t *pdir = get_pdir();

    for( word_t pd = CONFIG_WEDGE_VIRT/PAGEDIR_SIZE;
	                pd < HYPERVISOR_VIRT_START/PAGEDIR_SIZE; pd++ )
    {
	if( !pdir[pd].is_valid() )
	    continue;
	if( pd == pgent_t::get_pdir_idx(word_t(pgtab_region)) )
	    continue; // Don't globalize recurisvely mapped ptabs!

	pgent_t *ptab = get_ptab( pd );
	for( word_t pt = 0; pt < PAGE_SIZE/sizeof(pgent_t); pt++ )
	{
	    if( !ptab[pt].is_valid() )
		continue;
	    word_t vaddr = pd*PAGEDIR_SIZE + pt*PAGE_SIZE;
	    pgent_t pgent = ptab[pt];
	    pgent.set_global();
	    bool good = mmop_queue.ptab_update( 
		get_pgent_maddr(vaddr), pgent.get_raw(), true );
	    if( !good )
		PANIC( "Unable to enable global pages for the wedge." );
	}
    }
#endif
}

void xen_memory_t::map_boot_page( word_t vaddr, word_t maddr, bool read_only )
{
    pgent_t pgent;
    bool good;

    pgent.clear(); pgent.set_valid();
    ON_KAXEN_GLOBAL_PAGES( pgent.set_global() );
    pgent.set_kernel(); pgent.set_address( maddr );
    if( read_only )
	pgent.set_read_only();
    else
	pgent.set_writable();

    pgent_t pdent = get_pdir()[ pgent_t::get_pdir_idx(vaddr) ];
    
    if( !pdent.is_valid() )
	alloc_boot_ptab( vaddr );
        
    good = mmop_queue.ptab_update( get_pgent_maddr(vaddr), pgent.get_raw() );
    good &= mmop_queue.invlpg( vaddr, true );
    if (!good) 
	PANIC( "Unable to add a page table mapping." );
}

void xen_memory_t::alloc_boot_region( word_t vaddr, word_t size )
{
    //printf( "alloc_boot_region " << (void*)vaddr << " " << (void*)size << "\n");

    if( size % PAGE_SIZE )
	size = (size + PAGE_SIZE) & PAGE_MASK;

    pgent_t *pdir = get_pdir();
    while( size ) {
	word_t pd = pgent_t::get_pdir_idx(vaddr);
	if( !pdir[pd].is_valid() )
	    alloc_boot_ptab( vaddr );
	if( pdir[pd].is_superpage() )
	    PANIC( "Unexpected super page mapping." );

	pgent_t *ptab = get_ptab( pd );
	word_t pt = pgent_t::get_ptab_idx(vaddr);
	if( ptab[pt].is_valid() )
	    PANIC( "A mapping already exists." );

	word_t maddr = allocate_boot_page();
	if( debug_alloc_boot_region )
	    printf( "Allocating unused page: "
		<< (void *)vaddr << " --> " << (void *)maddr << '\n';
	map_boot_page( vaddr, allocate_boot_page() );

	vaddr += PAGE_SIZE;
	size -= PAGE_SIZE;
    }
}

void xen_memory_t::alloc_boot_ptab( word_t vaddr )
{
    pgent_t pdent;
    bool good;

    pdent.clear(); pdent.set_kernel(); pdent.set_writable(); pdent.set_valid();
    pdent.set_address( allocate_boot_page() );

    //printf( "alloc boot ptab: " << (void*)pdent.get_address() << "\n");

    if( debug_alloc_boot_ptab ) {
	printf( "Adding new leaf page table:\n");
	dump_pgent( 1, vaddr, pdent, PAGE_SIZE );
    }

    good  = mmop_queue.ptab_update( get_pdent_maddr(vaddr),
				    pdent.get_raw() );
    good &= mmop_queue.invlpg( vaddr );
    good &= mmop_queue.pin_table( pdent.get_address(), 0, true );
    if( !good )
	PANIC( "Unable to add a page table." );
}

void xen_memory_t::alloc_remaining_boot_pages()
{
    word_t pd = 0;

    for( pd = 0; pd < HYPERVISOR_VIRT_START/PAGEDIR_SIZE; pd++ )
    {
	if( !get_pdir()[pd].is_valid() ) {
	    if( pd > 1 )
		return; // Stop allocating after 4MB.
	    if( unallocated_pages() < 2 )
		return; // We need at least two pages.
	    alloc_boot_ptab( pd*PAGEDIR_SIZE );
	}
	if( get_pdir()[pd].is_superpage() )
	    continue;

	pgent_t *ptab = get_ptab(pd);
	for( word_t pt = 0; pt < PAGE_SIZE/sizeof(pgent_t); pt++ )
	{
	    if( ptab[pt].is_valid() )
		continue;
	    if( unallocated_pages() < 1 )
		return;

	    word_t vaddr = pd*PAGEDIR_SIZE + pt*PAGE_SIZE;
	    word_t maddr;
/*	    if( is_device_overlap(vaddr) ) {
		maddr = vaddr;
		if( debug_alloc_remaining )
		    printf( "1:1 device mapping: " << (void *)maddr << '\n';
	    }
	    else */{
		maddr = allocate_boot_page();
		if( debug_alloc_remaining )
		    printf( "Allocating unused page: "
			<< (void *)vaddr << " --> " << (void *)maddr << '\n';
	    }
	    map_boot_page( vaddr, maddr );
	}
    }
}

void xen_memory_t::remap_boot_region( 
	word_t boot_addr, word_t page_cnt, word_t new_vaddr )
{
    pgent_t *pdir = get_pdir();

    while( page_cnt ) {
	if( !pdir[pgent_t::get_pdir_idx(boot_addr)].is_valid() )
	    PANIC( "Invalid boot page directory entry." );
	pgent_t *ptab = get_ptab( pgent_t::get_pdir_idx(boot_addr) );
	pgent_t pgent = ptab[ pgent_t::get_ptab_idx(boot_addr) ];
	if( !pgent.is_valid() )
	    PANIC( "Invalid boot page table entry." );

	if( debug_remap_boot_region ) {
	    printf( "Remap source pgent:\n");
	    dump_pgent( 1, boot_addr, pgent, PAGE_SIZE );
	}

	if( !pdir[ pgent_t::get_pdir_idx(new_vaddr) ].is_valid() )
	    alloc_boot_ptab( new_vaddr );
	if( pdir[ pgent_t::get_pdir_idx(new_vaddr) ].is_superpage() )
	    PANIC( "Unexpected super page mapping." );

	pgent_t *new_ptab = get_ptab( pgent_t::get_pdir_idx(new_vaddr) );
	pgent_t new_pgent = new_ptab[ pgent_t::get_ptab_idx(new_vaddr) ];
	if( new_pgent.is_valid() )
	    PANIC( "Target mapping already exists at " << (void *)new_vaddr );

	if( debug_remap_boot_region ) {
	    printf( "Adding new page table entry:\n");
	    dump_pgent( 1, new_vaddr, pgent, PAGE_SIZE );
	}

	bool good;
	good  = mmop_queue.ptab_update( get_pgent_maddr(new_vaddr), 
					pgent.get_raw() );
	good &= mmop_queue.invlpg( new_vaddr );
	good &= mmop_queue.ptab_update( get_pgent_maddr(boot_addr), 0 ); // Remove old mapping.
	if( !good )
	    PANIC( "Unable to add a page table mapping." );

	boot_addr += PAGE_SIZE;
	new_vaddr += PAGE_SIZE;
	page_cnt--;
    }

    if( !mmop_queue.commit() )
	PANIC( "Unable to add a page table mapping." );
}

void xen_memory_t::init( word_t mach_mem_total )
{
    ASSERT( sizeof(mach_page_t) == sizeof(word_t) );

    this->total_mach_mem = mach_mem_total;
    boot_pdir_start_entry = word_t(-1);
    boot_mfn_list_allocated = 0;
    boot_mfn_list_start = 0;
    guest_phys_size = 0;
    pdir_maddr = 0;

    // Determine the last page dir entry needed by the wedge.
    extern word_t _end_wedge[];
    pdir_end_entry = word_t(_end_wedge);
    if( pdir_end_entry % PAGEDIR_SIZE )
	pdir_end_entry = (pdir_end_entry + PAGEDIR_SIZE) / PAGEDIR_SIZE;
    else
	pdir_end_entry /= PAGEDIR_SIZE;

    validate_boot_pdir();
    map_boot_pdir();
    globalize_wedge();
    init_segment_descriptors();

    // Init the phys-to-mach map.  It is automatically zeroed by the
    // allocation process.
    ASSERT( sizeof(mach_page_t) == sizeof(word_t) );
    alloc_boot_region( (word_t)xen_p2m_region, 
	    get_mfn_count()*sizeof(mach_page_t) );
}

void xen_memory_t::dump_pgent( 
	int level, word_t vaddr, pgent_t &pgent, word_t pgsize )
{
    while( level-- )
	printf( ' ';
    printf( (void *)vaddr << ":");
    if( !pgent.is_valid() ) {
	printf( " raw=" << (void *)pgent.get_raw() << ", <invalid>\n");
	return;
    }

    word_t maddr = pgent.get_address();
    word_t paddr = machine_to_phys_mapping[ pgent.get_address() >> PAGE_BITS ];
    if( paddr < get_guest_size() ) {
	printf( " phys=" << (void *)paddr;
	if( vaddr < CONFIG_WEDGE_VIRT )
	    maddr = xen_p2m_region[ paddr >> PAGE_BITS ].get_raw();
    }
    else
	printf( " phys=<invalid> ");

    printf( " mach=" << (void *)maddr << ' '
	<< (pgsize >= GB(1) ? pgsize >> 30 :
		pgsize >= MB(1) ? pgsize >> 20 : pgsize >> 10)
	<< (pgsize >= GB(1) ? 'G' : pgsize >= MB(1) ? 'M' : 'K')
	<< ' '
	<< (pgent.is_readable() ? 'r':'~')
	<< (pgent.is_writable() ? 'w':'~')
	<< (pgent.is_executable() ? 'x':'~')
	<< ":"
	<< (pgent.is_accessed() ? 'R':'~')
	<< (pgent.is_dirty()    ? 'W':'~')
	<< (pgent.is_executed() ? 'X':'~')
	<< (pgent.is_kernel() ? " kernel":" user")
	<< (pgent.is_global() ? " global":"")
	<< '\n';
}

void xen_memory_t::dump_active_pdir( bool pdir_only )
{
    pgent_t *pdir = get_pdir();
    for( word_t i = 0; i < HYPERVISOR_VIRT_START/PAGEDIR_SIZE; i++ )
//    for( word_t i = 0; i < PAGE_SIZE/sizeof(pgent_t); i++ )
    {
	if( !pdir[i].is_valid() )
	    continue;
	if( pdir[i].is_superpage() ) {
	    dump_pgent( 0, i*PAGEDIR_SIZE, pdir[i], PAGEDIR_SIZE );
	    continue;
	}
	else
	    dump_pgent( 0, i*PAGEDIR_SIZE, pdir[i], PAGE_SIZE );

	if( pdir_only )
	    continue;

	pgent_t *ptab = get_ptab(i);
	for( word_t j = 0; j < PAGE_SIZE/sizeof(pgent_t); j++ ) {
	    if( !ptab[j].is_valid() )
		continue;
	    dump_pgent( 1, i*PAGEDIR_SIZE + j*PAGE_SIZE, ptab[j], PAGE_SIZE );
	}
    }
}

word_t xen_memory_t::allocate_boot_page( bool panic_on_empty )
{
    word_t mfn, maddr;

    while( 1 ) {
	if( unallocated_pages() == 0 ) {
	    if( panic_on_empty )
		PANIC( "Insufficient boot memory." );
	    else
		return word_t(~0);
	}
	else if( boot_mfn_list_start > 0 ) {
	    // Consume the pages from the start of the mfn_list.
	    mfn = boot_mfn_list_start-1;
	    boot_mfn_list_start--;
	}
	else
	    mfn = boot_mfn_list_allocated + boot_mfn_list_start;
    	boot_mfn_list_allocated++;
	maddr = ((word_t *)xen_start_info.mfn_list)[mfn] << PAGE_BITS;
	if( is_device_memory(maddr) )
	    printf( "Suspicious: our VM was allocated a page that we thought "
		"was a device page: " << (void *)maddr << ", remaining=" << unallocated_pages() << '\n';
	else if( mfn == xen_start_info.shared_info )
	    printf( "Ugh: Xen's shared page is in our mfn list.\n");
	else
	    break;
    }

    // Temporarily map the page and zero it.
    map_boot_page( word_t(tmp_region), maddr );
    memzero( tmp_region, PAGE_SIZE );
    if( XEN_update_va_mapping( word_t(tmp_region), 0, UVMF_INVLPG))
	PANIC( "Unable to unmap the temporary region." );

    return maddr;
}

void xen_memory_t::init_m2p_p2m_maps()
{
    word_t pd, pt;
    bool finished;
    pgent_t *ptab, *pdir = get_pdir();
    word_t last_maddr = ~0;
    word_t phys_entry = 0;

    if( debug_contiguous )
	printf( "Machine memory regions:\n");

    /* The page tables have a contiguous set of mappings that represent
     * physical memory in the pre-boot environment.  We derive the
     * lower part of the physical-to-machine map from this contiguous
     * region.
     */
    guest_phys_size = 0;
    for( pd=0, finished=false; 
	    !finished && (pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE); pd++ )
    {
	if( !pdir[pd].is_valid() )
	    break;
	if( pdir[pd].is_superpage() )
	    PANIC( "Unsupported super page." );

	ptab = get_ptab(pd);

	for( pt=0; pt < PAGE_SIZE/sizeof(pgent_t); pt++ )
	{
	    if( !ptab[pt].is_valid() ) {
		finished = true;
		break;
	    }

	    // Update the p2m table.
	    ASSERT( phys_entry < get_mfn_count() );
	    xen_p2m_region[ phys_entry ].init( ptab[pt].get_address() );
	    
	    // Update the m2p table.
	    if( !mmop_queue.ptab_phys_update( ptab[pt].get_address(),
			phys_entry << PAGE_BITS, true) )
		PANIC( "Failed to update the Xen machine-to-physical map." );

    	    phys_entry++;
	    guest_phys_size = phys_entry * PAGE_SIZE;

	    if( last_maddr == word_t(~0) ) {
		last_maddr = ptab[pt].get_address();
		if( debug_contiguous )
		    printf( "  " << (void *)last_maddr << " --> ");
	    }
	    else if( debug_contiguous && 
		    ptab[pt].get_address() != (last_maddr + PAGE_SIZE) )
	    {
	       	printf( (void *)last_maddr << '\n'
	    	    << "  " << (void *)ptab[pt].get_address() << " --> ");
	    }

	    last_maddr = ptab[pt].get_address();
	    ASSERT( m2p(ptab[pt].get_address()) == ((phys_entry-1) << PAGE_BITS) );
	}
    }

    word_t unallocated_start = boot_mfn_list_allocated + boot_mfn_list_start;
    word_t *mfn_list = (word_t *)xen_start_info.mfn_list;
    word_t total = xen_start_info.nr_pages;
    if( debug_contiguous )
	printf( "(Sorting " << (total - unallocated_start) << " pages) ");
    for( word_t j = unallocated_start; j < total; j++ )
	for( word_t i = j + 1; i < total; i++ ) {
	    if( mfn_list[j] > mfn_list[i] ) {
		word_t tmp = mfn_list[j];
		mfn_list[j] = mfn_list[i];
		mfn_list[i] = tmp;
	    }
	}

    // The pre-boot environment doesn't necessarily contain virtual mappings
    // for all of the domain's physical memory.
    while( unallocated_pages() )
    {
	ASSERT( phys_entry < get_mfn_count() );
	word_t maddr = allocate_boot_page(false);
	if( maddr == word_t(~0) )
	    break;
	xen_p2m_region[ phys_entry ].init( maddr );

	if( !mmop_queue.ptab_phys_update(maddr, phys_entry << PAGE_BITS, true) )
	    PANIC( "Failed to update the Xen machine-to-physical map." );

	if( debug_contiguous && maddr != (last_maddr + PAGE_SIZE) )
	    printf( (void *)last_maddr << '\n'
		<< "  " << (void *)maddr << " --> ");

	last_maddr = maddr;
	phys_entry++;
	guest_phys_size = phys_entry * PAGE_SIZE;
	ASSERT( m2p(maddr) == ((phys_entry-1) << PAGE_BITS) );
    }
    if( debug_contiguous )
	printf( (void *)last_maddr << '\n';
}

void xen_memory_t::enable_guest_paging( word_t pdir_phys )
{
    // Note: paging isn't yet enabled, so we are using physical addresses 
    // for dereferencing the page directory and page tables.  We are
    // currently running on the boot page directory.  The guest is trying
    // to install a new page directory plus page tables.
    word_t pd, pt;
    pgent_t *new_pdir = (pgent_t *)pdir_phys;
    word_t new_pdir_maddr = p2m( pdir_phys );
    bool good = true;

    // Prepare for making the new page directory read-only in the 
    // out-going address space.
    word_t pdir_phys_maddr = get_pgent_maddr( pdir_phys );
    pgent_t pdir_phys_pgent = get_pgent( pdir_phys );
    pdir_phys_pgent.set_read_only();
    p2mpage( pdir_phys ).set_pgtable();
    p2mpage( pdir_phys ).set_pinned();

    // Make the new page directory read-only in the in-coming address space.
    word_t pdir_virt = guest_p2v(pdir_phys);
    new_pdir[ pgent_t::get_pdir_idx(pdir_virt) ].get_ptab()[ pgent_t::get_ptab_idx(pdir_virt) ].set_read_only();
    new_pdir[ pgent_t::get_pdir_idx(pdir_virt) ].get_ptab()[ pgent_t::get_ptab_idx(pdir_virt) ].set_xen_special();
    pdir_virt = pdir_phys;
    if( new_pdir[ pgent_t::get_pdir_idx(pdir_virt) ].is_valid() ) {
	new_pdir[ pgent_t::get_pdir_idx(pdir_virt) ].get_ptab()[ pgent_t::get_ptab_idx(pdir_virt) ].set_read_only();
	new_pdir[ pgent_t::get_pdir_idx(pdir_virt) ].get_ptab()[ pgent_t::get_ptab_idx(pdir_virt) ].set_xen_special();
    }

    // The guest page table probably contains mappings back to itself.
    // Render those mappings read-only.
    for( pd = 0; pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd++ )
    {
	if( !new_pdir[pd].is_valid() || new_pdir[pd].is_superpage() )
	    continue;
	word_t vaddr = new_pdir[pd].get_address() + get_vcpu().get_kernel_vaddr();
	new_pdir[ pgent_t::get_pdir_idx(vaddr) ].get_ptab()[ pgent_t::get_ptab_idx(vaddr) ].set_read_only();
	new_pdir[ pgent_t::get_pdir_idx(vaddr) ].get_ptab()[ pgent_t::get_ptab_idx(vaddr) ].set_xen_special();
	vaddr = new_pdir[pd].get_address();
	if( new_pdir[ pgent_t::get_pdir_idx(vaddr) ].is_valid() ) {
	    new_pdir[ pgent_t::get_pdir_idx(vaddr) ].get_ptab()[ pgent_t::get_ptab_idx(vaddr) ].set_read_only();
	    new_pdir[ pgent_t::get_pdir_idx(vaddr) ].get_ptab()[ pgent_t::get_ptab_idx(vaddr) ].set_xen_special();
	}
    }

    // Convert the guest page table to point at machine addresses.
    for( pd = 0; pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd++ )
    {
	if( !new_pdir[pd].is_valid() )
	    continue;
	if( new_pdir[pd].is_superpage() )
	    PANIC( "The guest is trying to use a super page." );

	pgent_t *ptab = new_pdir[pd].get_ptab();  // This is a phys address valid in our current address space.
	new_pdir[pd].set_address( p2m(new_pdir[pd].get_address()) );
	new_pdir[pd].set_xen_machine();

	word_t old_ptab_pgent_maddr = get_pgent_maddr( word_t(ptab) );
	pgent_t old_ptab_pgent = get_pgent( word_t(ptab) );
	mach_page_t &mpage = p2mpage( word_t(ptab) );
#if defined(CONFIG_KAXEN_UNLINK_AGGRESSIVE)
	mpage.set_pdir_entry( pd ); // Override the last setting.
#elif defined(CONFIG_KAXEN_WRITABLE_PGTAB)
	mpage.link_inc();
#endif
	if( mpage.is_pgtable() ) {
	    // Page table alias for a previously pinned ptab.
	    continue;
	}
	mpage.set_pgtable();
	mpage.set_pinned();
	old_ptab_pgent.set_read_only();

	for( pt = 0; pt < PAGE_SIZE/sizeof(pgent_t); pt++ )
	{
	    if( !ptab[pt].is_valid() )
		continue;
	    ptab[pt].set_address( p2m(ptab[pt].get_address()) );
	    ptab[pt].set_xen_machine();
	}

	ASSERT( old_ptab_pgent.get_address() == new_pdir[pd].get_address() );

	good &= mmop_queue.ptab_update( old_ptab_pgent_maddr,
					old_ptab_pgent.get_raw() );
	good &= mmop_queue.invlpg( word_t(ptab) );
	good &= mmop_queue.pin_table( old_ptab_pgent.get_address(), 0 );
    }

    good &= mmop_queue.commit();
    if( !good ) {
	PANIC( "Unable to pin a leaf page table." );
    }

    // Copy the page directory entries of the wedge into the new page table.
    // Copy all the boot entries too, since we need them to later unpin the
    // boot paging structures.
    for( pd = CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; 
	    pd < HYPERVISOR_VIRT_START/PAGEDIR_SIZE; pd++ )
    {
	new_pdir[pd] = get_pdir()[pd];
    }

    // Recursively map the page directory.
    new_pdir[ pgent_t::get_pdir_idx(word_t(pgtab_region)) ].set_address(new_pdir_maddr);

    this->pdir_maddr = new_pdir_maddr;

    // Make the new page directory read-only in the current address space,
    // to satisfy Xen's reference counting.
    // Plus pin the table.  And then tell Xen to switch the page directory.
    // printf( "new baseptr=" << (void*)new_pdir_maddr << "\n");

    good &= mmop_queue.ptab_update( pdir_phys_maddr, pdir_phys_pgent.x.raw );
    good &= mmop_queue.pin_table( new_pdir_maddr, 1 );
    good &= mmop_queue.set_baseptr( new_pdir_maddr );
    good &= mmop_queue.commit();
    if( !good )
    {
	printf( "Tried to install the page directory at MFN: "
	    << (void *)new_pdir_maddr << '\n';
	PANIC( "Unable to switch the page directory." );
    }

    unpin_boot_pdir();

#ifdef CONFIG_XEN_2_0
    // Validate everything.
    word_t kernel_offset = get_vcpu().get_kernel_vaddr();
    pgent_t *pdir2 = (pgent_t *)(pdir_phys + kernel_offset);
    for( pd = 0; pd < kernel_offset/PAGEDIR_SIZE; pd++ )
    {
	ASSERT( pdir2[pd].get_raw() == get_pdir()[pd].get_raw() );
	if( !get_pdir()[pd].is_valid() )
	    continue;
	pgent_t *ptab = get_ptab(pd);
	pgent_t *ptab2 = (pgent_t *)(kernel_offset + m2p(get_pdir()[pd].get_address()));
	for( pt = 0; pt < PAGE_SIZE/sizeof(pgent_t); pt++ )
	{
	    ASSERT( ptab[pt].get_raw() == ptab2[pt].get_raw() );
	    if( !ptab[pt].is_valid() )
		continue;
	    ASSERT( m2p(ptab[pt].get_address()) == (pd*PAGEDIR_SIZE + pt*PAGE_SIZE) );
	}
    }
    for( ; pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd++ )
    {
	ASSERT( pdir2[pd].get_raw() == get_pdir()[pd].get_raw() );
	if( !get_pdir()[pd].is_valid() )
	    continue;
	pgent_t *ptab = get_ptab(pd);
	pgent_t *ptab2 = (pgent_t *)(kernel_offset + m2p(get_pdir()[pd].get_address()));
	for( pt = 0; pt < PAGE_SIZE/sizeof(pgent_t); pt++ )
	{
	    ASSERT( ptab[pt].get_raw() == ptab2[pt].get_raw() );
	    if( !ptab[pt].is_valid() )
		continue;
	    ASSERT( m2p(ptab[pt].get_address()) == (pd*PAGEDIR_SIZE + pt*PAGE_SIZE - get_vcpu().get_kernel_vaddr()) );
	}
    }
#endif
}

void xen_memory_t::unpin_boot_pdir()
    // The boot page directory has mappings to guest OS pages, and these
    // mappings prevent Xen from using guest OS pages for page tables.
{
    bool good = true;

    // We must unmap the recursive page directory, to decrement the use count.
    word_t pdir_loopback = this->boot_pdir_maddr + 
	sizeof(pgent_t)*pgent_t::get_pdir_idx(word_t(pgtab_region));
    good &= mmop_queue.ptab_update( pdir_loopback, 0 );

    // Unpin all the page tables that used to back the guest's area of
    // the virtual address space.
    pgent_t *boot_pdir = get_boot_pdir();
    for( word_t pd = 0; pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd++ )
    {
	if( !boot_pdir[pd].is_valid() )
	    continue;

	//printf( "unpin table " << (void*)boot_pdir[pd].get_address() << "\n");
	good &= mmop_queue.unpin_table( boot_pdir[pd].get_address(), true );
    }

    // Unpin the boot pdir.
    good &= mmop_queue.unpin_table( this->boot_pdir_maddr );
   
    good &= mmop_queue.commit();
    if( !good )
	PANIC( "Unable to unpin the boot page tables." );
}


void xen_memory_t::init_segment_descriptors()
{
    // Allocate a page and temporarily map it.
    word_t maddr = allocate_boot_page(); // The page had better be cleared.
    map_boot_page( word_t(tmp_region), maddr );

    segdesc_t *seg_tbl = (segdesc_t *)tmp_region;

    segdesc_t d;
    d.x.raw = 0;

    d.x.code.granularity = 1;
    d.x.code.readable = 1;
    d.x.code.privilege = 1;
    d.x.code.type = 1;
    d.x.code.category = 1;
    d.x.code.default_bit = 1;
    d.x.code.present = 1;
    d.set_limit( HYPERVISOR_VIRT_START + MB(4) );
    d.set_base( 0 );

    seg_tbl[ XEN_CS_KERNEL >> 3 ] = d;
#if defined(GUEST_CS_BOOT_SEGMENT)
    seg_tbl[ GUEST_CS_BOOT_SEGMENT ] = d;
#endif

    d.x.data.type = 0;
    seg_tbl[ XEN_DS_KERNEL >> 3 ] = d;
#if defined(GUEST_DS_BOOT_SEGMENT)
    seg_tbl[ GUEST_DS_BOOT_SEGMENT ] = d;
#endif

    d.x.code.type = 1;
    d.x.code.privilege = 3;
    seg_tbl[ XEN_CS_USER >> 3 ] = d;

    d.x.data.type = 0;
    seg_tbl[ XEN_DS_USER >> 3 ] = d;

    // Remove the temporary mapping.  The table mustn't be mapped with
    // writable permissions, and we have no reason for a mapping to the page.
    if( XEN_update_va_mapping( word_t(tmp_region), 0, UVMF_INVLPG))
	PANIC( "Unable to unmap the temporary region." );

    // Install the segment descriptor table.
    word_t mfn_list[1];
    mfn_list[0] = maddr >> PAGE_BITS;
    if( XEN_set_gdt(mfn_list, PAGE_SIZE/sizeof(seg_tbl[0])) )
	PANIC( "Unable to install the segment descriptor table." );

    // Enable the new segments.
    __asm__ __volatile__ (
	"mov	%0, %%ss ;"	// Install SS.
	"mov	%1, %%ds ;"	// Install DS.
	"mov	%1, %%es ;"	// Install ES.
	"mov	%1, %%fs ;"	// Install ES.
	"mov	%1, %%gs ;"	// Install ES.
	"ljmp	%2, $1f ;"	// Install CS.
	"1: "
	: : "a"(XEN_DS_KERNEL), "b"(XEN_DS_USER), "i"(XEN_CS_KERNEL)
    );
}

bool xen_memory_t::map_device_memory( word_t vaddr, word_t maddr, bool boot)
{ 
    
    ASSERT(boot);
    
    if (vaddr != (word_t) tmp_region)
	ASSERT (!get_pdir()[ pgent_t::get_pdir_idx(vaddr) ].is_valid());
    
    map_boot_page(vaddr, maddr, false); 

    if (debug_map_device)
	printf( "Mapped boot device memory " << (void *) vaddr << "\n");
    return true;
}



bool xen_memory_t::unmap_device_memory( word_t vaddr, word_t maddr, bool boot)
{ 
    ASSERT(boot);
    bool ret = true;

    if (get_pdir()[ pgent_t::get_pdir_idx(vaddr) ].is_valid())
	ret = mmop_queue.ptab_update( get_pgent_maddr(vaddr), 0, false);
    
    ret &= mmop_queue.invlpg( vaddr, true );
	    
    if (!ret) 
	PANIC( "Unable to unmap boot device memory." );
	    
    if (debug_map_device)
	printf( "Unmapped boot device memory " << (void *) vaddr << "\n");

   
    return true;

}

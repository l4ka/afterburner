/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/amd64/memory_init.cc
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
#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(vcpulocal.h)

#include <memory.h>

#if 0
static const bool debug_remap_boot_region = false;
static const bool debug_alloc_boot_ptab = false;
static const bool debug_alloc_remaining = false;
static const bool debug_alloc_boot_region = false;
static const bool debug_map_device = false;
static const bool debug_contiguous = true;
#endif
static const bool debug_count_boot_pages = true;
static const bool debug_init_boot_ptables = true;

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


void xen_memory_t::globalize_wedge()
{
#if defined(CONFIG_KAXEN_GLOBAL_PAGES)
#error "Not ported!"
    pgent_t *pdir = get_mapping_base();

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

    con << "map_boot_page: vaddr " << (word_t*)vaddr << "  maddr " << maddr << '\n';
    // make sure there is a pgent
    alloc_boot_ptab( vaddr );

    con << "hello: " << (word_t*)boot_p2m( (word_t)get_boot_pgent_ptr( vaddr ) ) << "  " << (word_t*)pgent.get_raw() << '\n';
    good = mmop_queue.ptab_update( boot_p2m( (word_t)get_boot_pgent_ptr( vaddr ) ), pgent.get_raw() );
    good &= mmop_queue.invlpg( vaddr, true );
    con << "map: " << (word_t*)get_boot_pgent_ptr(vaddr)->get_raw() << '\n';
    if (!good) 
	PANIC( "Unable to add a page table mapping." );
}

void xen_memory_t::alloc_boot_region( word_t vaddr, word_t size )
{
    //con << "alloc_boot_region " << (void*)vaddr << " " << (void*)size << "\n";

    if( size % PAGE_SIZE )
	size = (size + PAGE_SIZE) & PAGE_MASK;

    UNIMPLEMENTED();
#if 0
    pgent_t *pdir = get_mapping_base();
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
	    con << "Allocating unused page: "
		<< (void *)vaddr << " --> " << (void *)maddr << '\n';
	map_boot_page( vaddr, allocate_boot_page() );

	vaddr += PAGE_SIZE;
	size -= PAGE_SIZE;
    }
#endif
}

void xen_memory_t::alloc_boot_ptab_b( pgent_t *e_)
{
    con << "alloc_boot_ptab_b: " << e_ << '\n';
    ASSERT( !e_->is_valid() );

    pgent_t e;
    e.clear(); e.set_valid();
    ON_KAXEN_GLOBAL_PAGES( e.set_global() );
    e.set_kernel();e.set_writable();

    word_t maddr = allocate_boot_page();
    e.set_address( maddr );
    
    bool good  = mmop_queue.ptab_update( boot_p2m( (word_t)e_ ),
				         e.get_raw() );
    map_boot_page( (word_t)boot_p2v( m2p( maddr ) ), maddr);
    good &= mmop_queue.commit();

    if( !good )
       PANIC( "Unable to add page table!" );
}

void xen_memory_t::alloc_boot_ptab( word_t vaddr )
{
    con << "alloc_boot_ptab: " << (word_t*)vaddr;
    pgent_t *pml4  = get_boot_mapping_base();
    pml4 += pgent_t::get_pml4_idx(vaddr);
    con << " pml4: " << pml4;
    if( !pml4->is_valid() )
       alloc_boot_ptab_b( pml4 );

    pgent_t *pdp   = (pgent_t *)m2p( pml4 -> get_address() );
    con << " pdp: " << pdp;
    pdp += pgent_t::get_pdp_idx(vaddr);
    if( !pdp->is_valid() )
       alloc_boot_ptab_b( pdp );

    pgent_t *pdir   = (pgent_t *)m2p( pdp -> get_address() );
    con << " pdir: " << pdir << '\n';
    pdir += pgent_t::get_pdir_idx(vaddr);
    if( !pdir->is_valid() )
       alloc_boot_ptab_b( pdir );

    // it is now guaranteed that we have a valid pgent
}

#if 0
void xen_memory_t::alloc_remaining_boot_pages()
{
    word_t pd = 0;

    for( pd = 0; pd < HYPERVISOR_VIRT_START/PAGEDIR_SIZE; pd++ )
    {
	if( !get_mapping_base()[pd].is_valid() ) {
	    if( pd > 1 )
		return; // Stop allocating after 4MB.
	    if( unallocated_pages() < 2 )
		return; // We need at least two pages.
	    alloc_boot_ptab( pd*PAGEDIR_SIZE );
	}
	if( get_mapping_base()[pd].is_superpage() )
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
		    con << "1:1 device mapping: " << (void *)maddr << '\n';
	    }
	    else */{
		maddr = allocate_boot_page();
		if( debug_alloc_remaining )
		    con << "Allocating unused page: "
			<< (void *)vaddr << " --> " << (void *)maddr << '\n';
	    }
	    map_boot_page( vaddr, maddr );
	}
    }
}
#endif

#if 0
void xen_memory_t::remap_boot_region( 
	word_t boot_addr, word_t page_cnt, word_t new_vaddr )
{
    pgent_t *pdir = get_mapping_base();

    while( page_cnt ) {
	if( !pdir[pgent_t::get_pdir_idx(boot_addr)].is_valid() )
	    PANIC( "Invalid boot page directory entry." );
	pgent_t *ptab = get_ptab( pgent_t::get_pdir_idx(boot_addr) );
	pgent_t pgent = ptab[ pgent_t::get_ptab_idx(boot_addr) ];
	if( !pgent.is_valid() )
	    PANIC( "Invalid boot page table entry." );

	if( debug_remap_boot_region ) {
	    con << "Remap source pgent:\n";
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
	    con << "Adding new page table entry:\n";
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
#endif

void xen_memory_t::count_boot_pages()
{
   // we are guaranteed to be started with a contiguous virtual memory region,
   // starting from address 0, that is 4MB aligned and a 1:1 mapping of our
   // pseudo-physical address space
   //
   // This does of course count a lot of unnecessary pages. We will have to
   // reclaim them later.

   unsigned long i1; // unsigned long to aid shifting
   pgent_t* pml4 = get_boot_mapping_base();
   // we don't test the highest mappings as we aliased ourselves thereto in
   // startup.cc
   for( i1 = 0;i1 < 510;++i1 )
      if( !pml4[i1].is_valid() )
	 break;
   ASSERT(i1 != 0);
   --i1;

   pgent_t* pdp = (pgent_t *)m2p( pml4[i1].get_address() );
   unsigned long i2;
   for( i2 = 0;i2 < 510;++i2 )
      if( !pdp[i2].is_valid() )
	 break;
   ASSERT(i2 != 0);
   --i2;

   pgent_t* pdir = (pgent_t *)m2p( pdp[i2].get_address() );
   unsigned long i3;
   for( i3 = 0;i3 < 510;++i3 )
      if( !pdir[i3].is_valid() )
	 break;
   ASSERT(i3 != 0);
   --i3;

   pgent_t* ptab = (pgent_t *)m2p( pdir[i3].get_address() );
   unsigned long i4;
   for( i4 = 0;i4 < 512;++i4 )
      if( !ptab[i4].is_valid() )
	 break;
   ASSERT(i4 != 0);
   --i4;

   word_t addr = i1 << PML4_BITS
               | i2 << PDP_BITS
	       | i3 << PAGEDIR_BITS
	       | i4 << PAGE_BITS;
   addr += PAGE_SIZE;

   boot_mfn_list_allocated = addr >> PAGE_BITS;

   if( debug_count_boot_pages )
      con << "nr boot pages: " << boot_mfn_list_allocated << '\n';
}

void xen_memory_t::init_tmp_static_split_region()
{
   // XXX Do we need that at all?
   //UNIMPLEMENTED();
}

static const unsigned slots = 2;
static char tmp_page[PAGE_SIZE * slots] __attribute__((__aligned__(PAGE_SIZE)));
void* xen_memory_t::map_boot_tmp( word_t ma, unsigned slot, bool rw )
{
   ASSERT( slot < slots );
   word_t r = (word_t)tmp_page + slot * PAGE_SIZE;

   pgent_t* p = get_boot_pgent_ptr( r );
   ASSERT( p );

   pgent_t pn = *p;
   pn.set_address( ma );
   if( !rw )
      pn.set_read_only();
   else
      pn.set_writable();
   //con << (word_t*)r << ' ' << (word_t*)pn.get_raw() << '\n';
   if( XEN_update_va_mapping( r, pn.get_raw(), UVMF_INVLPG) )
      return 0;

   return (void*)r;
}

word_t xen_memory_t::init_boot_ptables(unsigned level, word_t ptab)
{
   // OK, now this is a little tricky.. weird rather. Xen mapped us to low
   // addresses, as a typical 32 bit bootloader would do. We aliased a small
   // number of high address entries to make our code and data appear at the
   // right virtual addresses. The best thing now would probably be to
   // install a completely new page table, but that cannot be done easily, as
   // we do not even have access to a large portion of the machine memory.
   //
   // So what do we do? Well, we allocate space for one temporary page using
   // the compiler/linker. Then we map machine pages there, initialize them,
   // and insert them as page table entries.
   //
   // Note that this function is highly recursive, so we need to limit the
   // amount of stack space allocated.

   // TODO don't copy everything twice
   //      free unneeded pages
   //      update p2m/m2p tables?

   //con << level << " -- " << (word_t*)ptab << '\n';
   if( map_boot_tmp( ptab, 0, 0 ) == 0 )
   {
      if( debug_init_boot_ptables )
	 con << "shallowly copying " << (word_t*)ptab << '\n';
      // we cannot access this page, wich means it is a xen private mapping
      // --> return a shallow copy
      return ptab;
   }

   // first find us a new page.
   word_t copy = allocate_boot_page( true, false );

   if( level == 1 )
   {
      // leave page table, just plain copy
      pgent_t* pt = (pgent_t*) map_boot_tmp( ptab, 0 );
      pgent_t* cpt = (pgent_t*) map_boot_tmp( copy, 1 );
      memcpy( cpt, pt, PAGE_SIZE );
      return copy;
   }

   for( unsigned i = 0;i < PTAB_ENTRIES;++i )
   {
      // we need to remap each time, as we call ourself recursively
      pgent_t* pt = (pgent_t*) map_boot_tmp( ptab, 0, 0 );
      ASSERT( get_boot_pgent_ptr( (word_t)pt )->get_address() == ptab );
      pgent_t cpt = pt[i];

      if( !pt[i].is_valid() )
         continue;

      //con << level - 1 << " -- " << (word_t*)pt[i].get_address() << '\n';
      word_t addr = init_boot_ptables( level - 1, pt[i].get_address() );
      pgent_t* cptp = (pgent_t*) map_boot_tmp( copy, 1, 1 );
      *cptp = cpt;
      cptp->set_address( addr );
   }

   return copy;
}

void xen_memory_t::init( word_t mach_mem_total )
{
    ASSERT( sizeof(mach_page_t) == sizeof(word_t) );

    this->total_mach_mem = mach_mem_total;
    boot_mfn_list_allocated = 0;
    boot_mfn_list_start = 0;
    guest_phys_size = 0;
    mapping_base_maddr = 0;
    guest_phys_size = xen_start_info.nr_pages * PAGE_SIZE; // so that m2p can be used

    count_boot_pages(); // set boot_mfn_list_allocated

    this->mapping_base_maddr = this->boot_mapping_base_maddr =
       get_boot_pgent_ptr( (word_t)get_boot_mapping_base() ) -> get_address();
    con << "mapping base maddr: " << (word_t*)mapping_base_maddr << '\n';

    dump_active_pdir(1);

    word_t new_mapping_base = init_boot_ptables( 4, mapping_base_maddr );
    con << "new mapping base maddr: " << (word_t*)new_mapping_base << '\n';
    mmop_queue.set_baseptr( new_mapping_base, true );
    con << "test\n";

    init_tmp_static_split_region();

    return;

    // XXX TODO we have to create a sane (i.e. non-aliased) boot page table
    //          before doing anything serious

    word_t addr = allocate_boot_page();
    con << (word_t*)addr << '\n';
    dump_active_pdir(0);

#if 0
    map_boot_page( (word_t)tmp_region, addr );
    con << get_boot_pgent_ptr((word_t)tmp_region)->get_raw() << '\n';
    *((char*)tmp_region) = 0;
#endif

    addr = allocate_boot_page();
    con << (word_t*)addr << '\n';

    globalize_wedge();
    init_segment_descriptors();

#if 0
    // Init the phys-to-mach map.  It is automatically zeroed by the
    // allocation process.
    ASSERT( sizeof(mach_page_t) == sizeof(word_t) );
    alloc_boot_region( (word_t)xen_p2m_region, 
	    get_mfn_count()*sizeof(mach_page_t) );
#endif
}

void xen_memory_t::dump_pgent( 
	int level, word_t vaddr, pgent_t &pgent, word_t pgsize )
{
    while( level-- )
	con << "    ";
    con << (void *)vaddr << ":";
    if( !pgent.is_valid() ) {
	con << " raw=" << (void *)pgent.get_raw() << ", <invalid>\n";
	return;
    }

    word_t maddr = pgent.get_address();
    word_t paddr = m2p( pgent.get_address() );
    if( paddr < get_guest_size() ) {
	con << " phys=" << (void *)paddr;
#if 0
	if( vaddr < CONFIG_WEDGE_VIRT )
	    maddr = xen_p2m_region[ paddr >> PAGE_BITS ].get_raw();
#endif
    }
    else
	con << " phys=<invalid> ";

    con << " mach=" << (void *)maddr << ' '
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

void xen_memory_t::dump_pgent_b( pgent_t *pt, unsigned level, unsigned thr )
{
    if( level < thr )
	return;

    if( !pt->is_valid() )
	return;

    dump_pgent( 4 - level, (((word_t)pt) & 4095) / 8, *pt, PAGE_SIZE );

    if( level == 1 ) // leaf entry
	return;

    if( !is_our_maddr( pt->get_address() ))
       return; // not accessible

    pgent_t* next = (pgent_t*) m2p( pt->get_address() );
    for( unsigned long i = 0;i < PTAB_ENTRIES;++i)
	dump_pgent_b ( next + i, level - 1, thr );
}

void xen_memory_t::dump_active_pdir( bool pdir_only )
{
   con << "-------------begin" << (pdir_only ? " (short)" : " (long)")
       << " page table dump------------\n";
   pgent_t* pml4 = get_boot_mapping_base(); // XXX should be get_mapping_base
   for( unsigned long i = 0;i < PTAB_ENTRIES;++i )
      dump_pgent_b( pml4 + i, 4, pdir_only ? 2 : 0 );
   con << "-------------page table dump finished---------\n";
}

word_t xen_memory_t::allocate_boot_page( bool panic_on_empty, bool zero )
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
	    con << "Suspicious: our VM was allocated a page that we thought "
		"was a device page: " << (void *)maddr << ", remaining=" << unallocated_pages() << '\n';
	else if( mfn == xen_start_info.shared_info )
	    con << "Ugh: Xen's shared page is in our mfn list.\n";
	else
	    break;
    }

    if( !zero )
       return maddr;

    con << "allocate_boot_page: " << maddr << '\n';
    // Temporarily map the page and zero it.
    map_boot_page( word_t(tmp_region), maddr );
    con << "foo: " << (word_t*)get_boot_pgent_ptr((word_t)tmp_region)->get_raw() << '\n';
    memzero( tmp_region, PAGE_SIZE );
    con << "bar\n";
    if( XEN_update_va_mapping( word_t(tmp_region), 0, UVMF_INVLPG))
	PANIC( "Unable to unmap the temporary region." );

    return maddr;
}

#if 0
void xen_memory_t::init_m2p_p2m_maps()
{
    word_t pd, pt;
    bool finished;
    pgent_t *ptab, *pdir = get_mapping_base();
    word_t last_maddr = ~0;
    word_t phys_entry = 0;

    if( debug_contiguous )
	con << "Machine memory regions:\n";

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
		    con << "  " << (void *)last_maddr << " --> ";
	    }
	    else if( debug_contiguous && 
		    ptab[pt].get_address() != (last_maddr + PAGE_SIZE) )
	    {
	       	con << (void *)last_maddr << '\n'
	    	    << "  " << (void *)ptab[pt].get_address() << " --> ";
	    }

	    last_maddr = ptab[pt].get_address();
	    ASSERT( m2p(ptab[pt].get_address()) == ((phys_entry-1) << PAGE_BITS) );
	}
    }

    word_t unallocated_start = boot_mfn_list_allocated + boot_mfn_list_start;
    word_t *mfn_list = (word_t *)xen_start_info.mfn_list;
    word_t total = xen_start_info.nr_pages;
    if( debug_contiguous )
	con << "(Sorting " << (total - unallocated_start) << " pages) ";
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
	    con << (void *)last_maddr << '\n'
		<< "  " << (void *)maddr << " --> ";

	last_maddr = maddr;
	phys_entry++;
	guest_phys_size = phys_entry * PAGE_SIZE;
	ASSERT( m2p(maddr) == ((phys_entry-1) << PAGE_BITS) );
    }
    if( debug_contiguous )
	con << (void *)last_maddr << '\n';
}
#endif

#if 0
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
	new_pdir[pd] = get_mapping_base()[pd];
    }

    // Recursively map the page directory.
    new_pdir[ pgent_t::get_pdir_idx(word_t(pgtab_region)) ].set_address(new_pdir_maddr);

    this->pdir_maddr = new_pdir_maddr;

    // Make the new page directory read-only in the current address space,
    // to satisfy Xen's reference counting.
    // Plus pin the table.  And then tell Xen to switch the page directory.
    // con << "new baseptr=" << (void*)new_pdir_maddr << "\n";

    good &= mmop_queue.ptab_update( pdir_phys_maddr, pdir_phys_pgent.x.raw );
    good &= mmop_queue.pin_table( new_pdir_maddr, 1 );
    good &= mmop_queue.set_baseptr( new_pdir_maddr );
    good &= mmop_queue.commit();
    if( !good )
    {
	con << "Tried to install the page directory at MFN: "
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
	ASSERT( pdir2[pd].get_raw() == get_mapping_base()[pd].get_raw() );
	if( !get_mapping_base()[pd].is_valid() )
	    continue;
	pgent_t *ptab = get_ptab(pd);
	pgent_t *ptab2 = (pgent_t *)(kernel_offset + m2p(get_mapping_base()[pd].get_address()));
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
	ASSERT( pdir2[pd].get_raw() == get_mapping_base()[pd].get_raw() );
	if( !get_mapping_base()[pd].is_valid() )
	    continue;
	pgent_t *ptab = get_ptab(pd);
	pgent_t *ptab2 = (pgent_t *)(kernel_offset + m2p(get_mapping_base()[pd].get_address()));
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
#endif

#if 0
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
    pgent_t *boot_pdir = get_boot_mapping_base();
    for( word_t pd = 0; pd < CONFIG_WEDGE_VIRT/PAGEDIR_SIZE; pd++ )
    {
	if( !boot_pdir[pd].is_valid() )
	    continue;

	//con << "unpin table " << (void*)boot_pdir[pd].get_address() << "\n";
	good &= mmop_queue.unpin_table( boot_pdir[pd].get_address(), true );
    }

    // Unpin the boot pdir.
    good &= mmop_queue.unpin_table( this->boot_pdir_maddr );
   
    good &= mmop_queue.commit();
    if( !good )
	PANIC( "Unable to unpin the boot page tables." );
}
#endif


void xen_memory_t::init_segment_descriptors()
{
#if 0
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
#endif
}

#if 0
bool xen_memory_t::map_device_memory( word_t vaddr, word_t maddr, bool boot)
{ 
    
    ASSERT(boot);
    
    if (vaddr != (word_t) tmp_region)
	ASSERT (!get_mapping_base()[ pgent_t::get_pdir_idx(vaddr) ].is_valid());
    
    map_boot_page(vaddr, maddr, false); 

    if (debug_map_device)
	con << "Mapped boot device memory " << (void *) vaddr << "\n";
    return true;
}
#endif



#if 0
bool xen_memory_t::unmap_device_memory( word_t vaddr, word_t maddr, bool boot)
{ 
    ASSERT(boot);
    bool ret = true;

    if (get_mapping_base()[ pgent_t::get_pdir_idx(vaddr) ].is_valid())
	ret = mmop_queue.ptab_update( get_pgent_maddr(vaddr), 0, false);
    
    ret &= mmop_queue.invlpg( vaddr, true );
	    
    if (!ret) 
	PANIC( "Unable to unmap boot device memory." );
	    
    if (debug_map_device)
	con << "Unmapped boot device memory " << (void *) vaddr << "\n";

   
    return true;

}
#endif

/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/kaxen/backend.cc
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

#include INC_ARCH(cycles.h)

#include <console.h>
#include <debug.h>
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(backend.h)

#include INC_ARCH(vmiTypes.h)
#include INC_ARCH(vmiRom.h)

#include <burn_counters.h>

static const bool debug_coherent=1;
static const bool debug_iret_user=0;
static const bool debug_set_pte = 0;
static const bool debug_get_pte = 0;

DECLARE_BURN_COUNTER(iret_user);
DECLARE_BURN_COUNTER(esp0_switch);
DECLARE_BURN_COUNTER(fpu_protect);
DECLARE_BURN_COUNTER(pte_read);
DECLARE_PERF_COUNTER(pte_read_cycles);
DECLARE_BURN_COUNTER(pte_read_simple);
DECLARE_BURN_COUNTER(pte_read_and_clear);
DECLARE_BURN_COUNTER(pte_test_and_clear);
DECLARE_BURN_COUNTER(pte_set);
DECLARE_BURN_COUNTER(pgd_set);
DECLARE_BURN_COUNTER(unpin_dma);
DECLARE_BURN_COUNTER(pte_xchg);

/* Note: The Xen hypercall burn counters are declared in ascending order
 * of their hypercall IDs, so that they are printed in ascending order.
 */
DECLARE_BURN_COUNTER( XEN_set_trap_table	);
DECLARE_PERF_COUNTER( XEN_set_trap_table_cycles	);
DECLARE_BURN_COUNTER( XEN_mmu_update		);
DECLARE_PERF_COUNTER( XEN_mmu_update_cycles	);
DECLARE_BURN_COUNTER( XEN_set_gdt		);
DECLARE_PERF_COUNTER( XEN_set_gdt_cycles	);
DECLARE_BURN_COUNTER( XEN_stack_switch		);
DECLARE_PERF_COUNTER( XEN_stack_switch_cycles	);
DECLARE_BURN_COUNTER( XEN_set_callbacks		);
DECLARE_PERF_COUNTER( XEN_set_callbacks_cycles	);
DECLARE_BURN_COUNTER( XEN_fpu_taskswitch	);
DECLARE_PERF_COUNTER( XEN_fpu_taskswitch_cycles	);
DECLARE_BURN_COUNTER( XEN_sched_op		);
DECLARE_PERF_COUNTER( XEN_sched_op_cycles	);
DECLARE_BURN_COUNTER( XEN_dom0_op		);
DECLARE_PERF_COUNTER( XEN_dom0_op_cycles	);
DECLARE_BURN_COUNTER( XEN_set_debugreg		);
DECLARE_PERF_COUNTER( XEN_set_debugreg_cycles	);
DECLARE_BURN_COUNTER( XEN_get_debugreg		);
DECLARE_PERF_COUNTER( XEN_get_debugreg_cycles	);
DECLARE_BURN_COUNTER( XEN_update_descriptor	);
DECLARE_PERF_COUNTER( XEN_update_descriptor_cycles );
DECLARE_BURN_COUNTER( XEN_set_fast_trap		);
DECLARE_PERF_COUNTER( XEN_set_fast_trap_cycles	);
DECLARE_BURN_COUNTER( XEN_dom_mem_op		);
DECLARE_PERF_COUNTER( XEN_dom_mem_op_cycles	);
DECLARE_BURN_COUNTER( XEN_multicall		);
DECLARE_PERF_COUNTER( XEN_multicall_cycles	);
DECLARE_BURN_COUNTER( XEN_update_va_mapping	);
DECLARE_PERF_COUNTER( XEN_update_va_mapping_cycles );
DECLARE_BURN_COUNTER( XEN_set_timer_op		);
DECLARE_PERF_COUNTER( XEN_set_timer_op_cycles	);
DECLARE_BURN_COUNTER( XEN_event_channel_op	);
DECLARE_PERF_COUNTER( XEN_event_channel_op_cycles );
DECLARE_BURN_COUNTER( XEN_xen_version		);
DECLARE_PERF_COUNTER( XEN_xen_version_cycles	);
DECLARE_BURN_COUNTER( XEN_console_io		);
DECLARE_PERF_COUNTER( XEN_console_io_cycles	);
DECLARE_BURN_COUNTER( XEN_physdev_op		);
DECLARE_PERF_COUNTER( XEN_physdev_op_cycles	);
DECLARE_BURN_COUNTER( XEN_grant_table_op	);
DECLARE_PERF_COUNTER( XEN_grant_table_op_cycles	);
DECLARE_BURN_COUNTER( XEN_vm_assist		);
DECLARE_PERF_COUNTER( XEN_vm_assist_cycles	);
DECLARE_BURN_COUNTER( XEN_update_va_mapping_otherdomain );
DECLARE_PERF_COUNTER( XEN_update_va_mapping_otherdomain_cycles );
DECLARE_BURN_COUNTER( XEN_switch_vm86		);
DECLARE_PERF_COUNTER( XEN_switch_vm86_cycles	);

INLINE pgent_t get_guest_pde( pgent_t pdent )
{
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());

    if( pdent.is_xen_machine() )
    {
	INC_BURN_COUNTER(pte_read);
	pdent.set_address( xen_memory.m2p(pdent.get_address()) );
	pdent.xen_reset();
    }
    else
	INC_BURN_COUNTER(pte_read_simple);

    ADD_PERF_COUNTER(pte_read_cycles, get_cycles()-cycles);
    return pdent;
}

INLINE pgent_t get_guest_pte( pgent_t pgent )
{
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    if( pgent.is_valid_xen_machine() )
    {
	INC_BURN_COUNTER(pte_read);
	pgent.set_address( xen_memory.m2p(pgent.get_address()) );
    }
    else
	INC_BURN_COUNTER(pte_read_simple);

    ADD_PERF_COUNTER(pte_read_cycles, get_cycles()-cycles);
    return pgent;
}

extern "C" word_t SECTION(".text.pte")
backend_pte_read_patch( pgent_t *pgent_old )
{
    // Note: Even though we accept a pointer to a pgent, the pointer isn't
    // necessarily pointing within a page table.  It could point at a 
    // value on the stack for example.
    return get_guest_pte( *pgent_old ).get_raw();
}

extern "C" word_t SECTION(".text.pte")
backend_pgd_read_patch( pgent_t *pdent )
{
    // Note: Even though we accept a pointer to a pgent, the pointer isn't
    // necessarily pointing within a page table.  It could point at a 
    // value on the stack for example.
    return get_guest_pde( *pdent ).get_raw();
}

#if defined(CONFIG_VMI_SUPPORT)

DECLARE_BURN_COUNTER(vmi_set);

extern "C" word_t SECTION(".text.pte")
backend_pte_normalize_patch( pgent_t pgent, int level )
{
    if (debug_get_pte)
	printf( "backend_normalize " << (void*) pgent.get_raw()
	    << " l=" << level << "\n");

    return level == 2 ? 
	get_guest_pte( pgent ).get_raw() :
	get_guest_pde( pgent ).get_raw();
}

extern "C" void
vmi_get_pxe_ext( burn_clobbers_frame_t *frame )
{
    	printf( "vmi_get_pxe_ext val=" << (void*)frame->eax << " level=" 
	    << (void*)frame->edx << " ret=" 
	    << (void*)frame->guest_ret_address << '\n';
	asm("int3");
}

extern "C" word_t SECTION(".text.pte")
backend_pde_normalize_patch( pgent_t pgent )
{
    return get_guest_pde( pgent ).get_raw();
}

extern "C" word_t SECTION(".text.pte")
backend_vmi_read_patch( pgent_t *pgent )
{
    // Note: Even though we accept a pointer to a pgent, the pointer isn't
    // necessarily pointing within a page table.  It could point at a 
    // value on the stack for example.

    return get_guest_pte( *pgent ).get_raw();
}

static inline pgent_t u32_to_pgent(u32_t val)
{
    pgent_t pgent;
    pgent.x.raw = val;
    return pgent;
}

/* mov %eax, (%edx) */
extern "C" void
vmi_set_pte_ext( burn_clobbers_frame_t *frame )
{
    if (debug_set_pte) 
	printf( "vmi_set_pte_ext pteptr=" << (void*)frame->eax << " pteval=" 
	    << (void*)frame->edx << " level=" << frame->ecx << " ret=" 
	    << (void*)frame->guest_ret_address << '\n';
    
    pgent_t pgent = u32_to_pgent(frame->edx);

#if defined(CONFIG_WEDGE_KAXEN)
    /* Silly Xen can't handle global bits.  And OF COURSE they don't
     * clear them, nooooo way... they barf */ 
    pgent.x.fields.global = 0; 
#endif

    xen_memory.change_pgent( (pgent_t*)frame->eax, pgent, frame->ecx == 2);

    if( get_cpu().get_irq_vectors() )
	get_cpu().prepare_interrupt_redirect();
}

/* LOCK bts %edx, (%eax); sbb %edx, %edx; mov %edx, %eax */
extern "C" void
vmi_test_and_set_pte_bit_ext( burn_clobbers_frame_t *frame )
{
    if (debug_set_pte)
	printf( "vmi_test_and_set_pte_bit_ext " << (void*)frame->edx 
	    << " " << (void*)frame->eax << "\n");

    u32_t val = *(u32_t *)frame->eax;

    if( (val & (1 << frame->edx)) == 0 ) {
	xen_memory.change_pgent( (pgent_t*)frame->eax, 
		u32_to_pgent(val | (1 << frame->edx)), true );
	if( get_cpu().get_irq_vectors() )
    	    get_cpu().prepare_interrupt_redirect();
    }

    frame->eax = val & (1 << frame->edx);
}

#if 0
extern "C" void
vmi_allocate_page_ext( burn_clobbers_frame_t *frame )
{
    printf( "VMI_AllocatePage " << (void*)frame->eax << ", " 
	<< (void*)frame->edx << ", " << (void*)frame->ecx << ", " 
	<< (void*)frame->params[0] << "\n");

    if ((frame->edx & (VMI_PAGE_PD | VMI_PAGE_PT)) == 0)
	return;

    mach_page_t &mpage = xen_memory.p2mpage(frame->eax << PAGE_BITS);

    ASSERT(!mpage.is_pgtable());

    mpage.set_pgtable();
    //mpage.set_ptab_level(frame->edx & VMI_PAGE_PD ? 1 : 0);
}

extern "C" void
vmi_release_page_ext( burn_clobbers_frame_t *frame )
{
    printf( "VMI_ReleasePage " << (void*)frame->eax << ", " 
	<< (void*)frame->edx << "\n");

    if ((frame->edx & (VMI_PAGE_PD | VMI_PAGE_PT)) == 0)
	return;

    mach_page_t &mpage = xen_memory.p2mpage(frame->eax << PAGE_BITS);

    ASSERT(mpage.is_pgtable());
    //mpage.set_normal();
}
#endif /* #if 0 */

#endif /* CONFIG_VMI_SUPPORT */


extern "C" void SECTION(".text.pte")
backend_pte_write_patch( pgent_t new_pgent, pgent_t *old_pgent )
{
    printf("hu? %p\n", old_pgent);
    INC_BURN_COUNTER(pte_set);

    xen_memory.change_pgent( old_pgent, new_pgent, true );

    if( get_cpu().get_irq_vectors() )
	get_cpu().prepare_interrupt_redirect();
}

extern "C" void SECTION(".text.pte")
backend_pte_or_patch( word_t bits, pgent_t *old_pgent )
{
    UNIMPLEMENTED();
#if 0
    DEBUGGER_ENTER(0);
    word_t old_val = old_pgent->get_raw();
    word_t new_val = old_val | bits;
    if( new_val != old_val ) {
	pgent_t new_pgent = {x: {raw: new_val}};
	xen_memory.change_pgent( old_pgent, new_pgent, true );
	if( get_cpu().get_irq_vectors() )
    	    get_cpu().prepare_interrupt_redirect();
    }
#endif
}

extern "C" void SECTION(".text.pte")
backend_pte_and_patch( word_t bits, pgent_t *old_pgent )
{
    UNIMPLEMENTED();
#if 0
    DEBUGGER_ENTER(0);
    word_t old_val = old_pgent->get_raw();
    word_t new_val = old_val & bits;
    if( new_val != old_val ) {
	pgent_t new_pgent = {x: {raw: new_val}};
	xen_memory.change_pgent( old_pgent, new_pgent, true );
	if( get_cpu().get_irq_vectors() )
    	    get_cpu().prepare_interrupt_redirect();
    }
#endif
}

extern "C" void SECTION(".text.pte")
backend_pgd_write_patch( pgent_t new_pgent, pgent_t *old_pgent )
{
    INC_BURN_COUNTER(pgd_set);

    xen_memory.change_pgent( old_pgent, new_pgent, false );

    if( get_cpu().get_irq_vectors() )
	get_cpu().prepare_interrupt_redirect();
}

extern "C" word_t SECTION(".text.pte")
backend_pte_test_clear_patch( word_t bit, pgent_t *pgent )
{
    UNIMPLEMENTED();
#if 0
    word_t val = pgent->get_raw();
    word_t old_bit = (val >> bit) & 1;

    INC_BURN_COUNTER(pte_test_and_clear);

    if( old_bit ) {
	val &= ~(1 << bit);
	pgent_t new_pgent = {x: {raw: val}};
	xen_memory.change_pgent( pgent, new_pgent, true );
	if( get_cpu().get_irq_vectors() )
    	    get_cpu().prepare_interrupt_redirect();
    }
#endif
    return 0;
}

extern "C" word_t SECTION(".text.pte")
backend_pte_xchg_patch( word_t new_val, pgent_t *pgent )
{
    UNIMPLEMENTED();
#if 0
    word_t old_val = pgent->get_raw();

    INC_BURN_COUNTER(pte_xchg);

    ASSERT( new_val == 0 );
    pgent_t new_pgent = {x: {raw: new_val}};
    xen_memory.change_pgent( pgent, new_pgent, true );

    if( get_cpu().get_irq_vectors() )
	get_cpu().prepare_interrupt_redirect();
#endif
    return 0;
}

word_t backend_dma_coherent_check( word_t phys, word_t size )
{
    if( debug_coherent )
	printf( "dma coherent?: %p, size: %lu\n", phys, size );

    if( xen_memory_t::is_device_overlap(phys) || (size <= PAGE_SIZE) )
	return 1;

    word_t end = (phys + size + (PAGE_SIZE - 1)) & PAGE_MASK;
    word_t last_dma = xen_memory.p2m( phys );
    for( word_t i = phys + PAGE_SIZE; i < end; i += PAGE_SIZE ) {
	word_t dma = xen_memory.p2m( i );
	if( dma != last_dma + PAGE_SIZE ) {
	    printf( "non coherent\n");
	    DEBUGGER_ENTER();
	    return 0;
	}
    }

    return 1;
}

word_t backend_phys_to_dma_hook( word_t phys, word_t size )
{
    dprintf(debug_dma, "phys to dma: %p, size: %lu", phys, size );

    if( xen_memory_t::is_device_overlap(phys) )
	return phys;

#if !defined(CONFIG_OPTIMIZE)
    if( size > PAGE_SIZE ) {
	word_t end = (phys + size + (PAGE_SIZE - 1)) & PAGE_MASK;
	word_t last_dma = xen_memory.p2m( phys );
	for( word_t i = phys + PAGE_SIZE; i < end; i += PAGE_SIZE ) {
	    word_t dma = xen_memory.p2m( i );
	    if( dma != last_dma + PAGE_SIZE )
		PANIC( "Unimplemented: non-contiguous DMA region"
		       ", size %lu, phys addr %p, dma addrs %p --> %p"
		       ", called from %p",
		       size, phys, last_dma, dma,
		       __builtin_return_address(0) );
	    last_dma = dma;
	}
    }
#endif

    mach_page_t &mpage = xen_memory.p2mpage( phys );
    if( mpage.is_pinned() ) {
	INC_BURN_COUNTER(unpin_dma);
	dprintf(debug_dma, "dma unpin\n");
	xen_memory.unpin_page( mpage, phys );
    }
    ASSERT( !mpage.is_pinned() );
    mpage.set_normal();

    return mpage.get_address() | (phys & ~PAGE_MASK);
}

word_t backend_dma_to_phys_hook( word_t dma )
{
    dprintf(debug_dma, "dma to phys: %p\n", dma );

    if( xen_memory_t::is_device_overlap(dma) )
	return dma;

    return xen_memory.m2p(dma) | (dma & ~PAGE_MASK);
}

void NORETURN SECTION(".text.iret") 
backend_activate_user( user_frame_t *user_frame )
{
    UNIMPLEMENTED();
#if 0
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;

    INC_BURN_COUNTER(iret_user);

#if defined(CONFIG_KAXEN_UNLINK_AGGRESSIVE) || defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    cpu.cr2 = 0;
#endif

    ASSERT( cpu.get_tss()->esp0 );
    backend_esp0_update();

    // Update emulated CPU state.
#if !defined(CONFIG_IA32_FAST_VECTOR)
    cpu.cs = user_frame->iret->cs;
    cpu.ss = user_frame->iret->ss;
    cpu.flags = user_frame->iret->flags;

    // For the segments, use Xen's pre-arranged values.
    user_frame->iret->cs = XEN_CS_USER;
    user_frame->iret->ss = XEN_DS_USER;
#endif

    if( debug_iret_user )
	printf( "iret to user, ip " << (void *)user_frame->iret->ip
	    << ", cs " << (void *)user_frame->iret->cs
	    << ", flags " << (void *)user_frame->iret->flags.x.raw
	    << ", sp " << (void *)user_frame->iret->sp
	    << ", ss " << (void *)user_frame->iret->ss << '\n';

    __asm__ __volatile__ (
	    "movl	%0, %%esp ;"
	    "popl	%%eax ;"
	    "popl	%%ecx ;"
	    "popl	%%edx ;"
	    "popl	%%ebx ;"
	    "addl	$4, %%esp ;"
	    "popl	%%ebp ;"
	    "popl	%%esi ;"
	    "popl	%%edi ;"
	    "addl	$16, %%esp ;" /* Skip flags and the iret redirect frame. */
	    "iret ;"
	    : /* outputs */
	    : /* inputs */
	      "r"(&user_frame->regs->x.raw[0])
	    );

    while(1); // No return.
#endif
}


INLINE u32_t word_text( char a, char b, char c, char d )
{
    return (a)<<0 | (b)<<8 | (c)<<16 | (d)<<24;
}

void backend_cpuid_override( 
	u32_t func, u32_t max_basic, u32_t max_extended,
	frame_t *regs )
{
    UNIMPLEMENTED();
#if 0
    if( (func > max_basic) && (func < 0x80000000) )
	func = max_basic;
    if( func > max_extended )
	func = max_extended;

    if( func <= max_basic )
    {
	switch( func )
	{
	    case 1:
		bit_clear(  3, regs->x.fields.ecx ); // Disable monitor/mwait.
		bit_clear( 17, regs->x.fields.edx ); // Disable PSE-36.
		bit_clear( 16, regs->x.fields.edx ); // Disable PAT.
#if !defined(CONFIG_KAXEN_GLOBAL_PAGES)
		bit_clear( 13, regs->x.fields.edx ); // Disable PGE.
#endif
		bit_clear( 11, regs->x.fields.edx ); // Disable SYSENTER/SYSEXIT
		bit_clear(  6, regs->x.fields.edx ); // Disable PAE.
		bit_clear(  3, regs->x.fields.edx ); // Disable PSE.
		break;
	}
    }
    else {
	switch( func )
	{
	    case 0x80000002:
		regs->x.fields.eax = word_text('K','a','X','e');
		regs->x.fields.ebx = word_text('n',' ','f','r');
		regs->x.fields.ecx = word_text('o','m',' ','U');
		regs->x.fields.edx = word_text('K','a',' ','<');
		break;
	    case 0x80000003:
		regs->x.fields.eax = word_text('h','t','t','p');
		regs->x.fields.ebx = word_text(':','/','/','l');
		regs->x.fields.ecx = word_text('4','k','a','.');
		regs->x.fields.edx = word_text('o','r','g','>');
		break;
	    case 0x80000004:
		regs->x.fields.eax = word_text(0,0,0,0);
		break;
	}
    }
#endif
}

#if defined(CONFIG_GUEST_PTE_HOOK)

void backend_set_pte_hook( pgent_t *old_pte, pgent_t new_pte, int level)
{
    INC_BURN_COUNTER(pte_set);
    xen_memory.change_pgent( old_pte, new_pte, level == 2 );
    if( get_cpu().get_irq_vectors() )
	get_cpu().prepare_interrupt_redirect();
}

word_t backend_pte_test_and_clear_hook( pgent_t *pgent, word_t bits )
{
    word_t val = pgent->get_raw();
    word_t old_bits = val & bits;

    if( old_bits ) {
	val &= ~bits;
	pgent_t new_pgent = {x: {raw: val}};
	backend_set_pte_hook( pgent, new_pgent, 2 );
    }

    INC_BURN_COUNTER(pte_test_and_clear);
    return old_bits != 0;
}

pgent_t backend_pte_get_and_clear_hook( pgent_t *pgent )
{
    pgent_t old = pgent->get_raw();

    pgent_t new_pgent = {x: {raw: 0}};
    backend_set_pte_hook( pgent, new_pgent, 2 );

    INC_BURN_COUNTER(pte_read_and_clear);
    return old;
}


void backend_free_pgd_hook( pgent_t *pgdir )
{
}
#endif

#if defined(CONFIG_GUEST_PTE_HOOK)
unsigned long SECTION(".text.pte")
backend_read_pte_hook( pgent_t pgent )
{
    return get_guest_pte(pgent).get_raw();
}
#endif


#if 0
void backend_esp0_sync( void )
{
    if( !backend_esp0_update() )
	return;

#if defined(CONFIG_IA32_FAST_VECTOR)
    static bool syscall_installed = false;
    static bool pgfault_installed = false;
    static const word_t syscall_vector = 0x80;
    static const word_t pgfault_vector = 14;
    if( EXPECT_FALSE(!syscall_installed) )
    {
	syscall_installed = true;
	ASSERT( syscall_vector < get_cpu().idtr.get_total_gates() );
	gate_t *idt = get_cpu().idtr.get_gate_table();
	gate_t &gate = idt[ syscall_vector ];
	ASSERT( gate.is_present() );
	ASSERT( gate.is_32bit() );
	ASSERT( gate.is_trap() );
	trap_info_t syscall_trap_table[2] =
	{ {syscall_vector, 3, XEN_CS_KERNEL, gate.get_offset()}, {0, 0, 0, 0} };
	if( XEN_set_trap_table(syscall_trap_table) ) {
	    printf( "Error: unable to install a new Xen trap table.\n");
	    DEBUGGER_ENTER(0);
	}
#ifdef CONFIG_XEN_2_0
	if( XEN_set_fast_trap(syscall_vector) ) {
	    printf( "Error: unable to install the Xen fast trap.\n");
	    DEBUGGER_ENTER(0);
	}
#endif
    }
    if( EXPECT_FALSE(!pgfault_installed) )
    {
	pgfault_installed = true;
	ASSERT( pgfault_vector < get_cpu().idtr.get_total_gates() );
	gate_t *idt = get_cpu().idtr.get_gate_table();
	gate_t &gate = idt[ pgfault_vector ];
	ASSERT( gate.is_present() );
	ASSERT( gate.is_32bit() );
	ASSERT( gate.is_interrupt() );
	extern word_t pgfault_gate_ip;
	pgfault_gate_ip = gate.get_offset();
    }
#endif
}
#endif

#if defined(CONFIG_GUEST_MODULE_HOOK)
bool backend_module_rewrite_hook( elf_ehdr_t *ehdr )
{
    UNIMPLEMENTED();
#if 0
    extern bool frontend_elf_rewrite( elf_ehdr_t *elf, word_t vaddr_offset, bool module );

    // In Linux, the ELF header has been modified: All section header
    // sh_addr fields are absolute addresses, pointing at the final
    // destination of the ELF data, and the sh_offset fields are invalid.  
    // Our ELF code needs valid sh_offset fields, and ignores the 
    // sh_addr fields.  So we munge the data a bit.
    word_t i, tmp;
    for( i = 0; i < ehdr->get_num_sections(); i++ ) {
	elf_shdr_t *shdr = &ehdr->get_shdr_table()[i];
	if( shdr->offset == 0 )
	    continue;
	tmp = shdr->offset;
	shdr->offset = (s32_t)shdr->addr - (s32_t)ehdr;
	shdr->addr = tmp;
    }

    bool result = frontend_elf_rewrite( ehdr, 0, true );

    // Return the section offsets to the values provided by Linux.
    for( i = 0; i < ehdr->get_num_sections(); i++ ) {
	elf_shdr_t *shdr = &ehdr->get_shdr_table()[i];
	if( shdr->offset == 0 )
	    continue;
	tmp = shdr->offset;
	shdr->addr = shdr->offset + (s32_t)ehdr;
	shdr->offset = tmp;
    }

    return result;
#endif
    return false;
}
#endif

#if defined(CONFIG_XEN_2_0)
time_t backend_get_unix_seconds()
{
    u32_t time_version;
    cycles_t last_tsc, tsc;
    time_t unix_seconds;
    volatile xen_time_info_t *time_info = xen_shared_info.get_time_info();

    do {
	time_version = time_info->time_version1;
	last_tsc = time_info->tsc_timestamp;
	unix_seconds = time_info->wc_sec;
    } while( time_version != time_info->time_version2 );

    tsc = get_cycles();
    unix_seconds += (tsc - last_tsc) / get_vcpu().cpu_hz;

    return unix_seconds;
}
#else
time_t backend_get_unix_seconds()
{
    volatile xen_wc_info_t *wc_info = xen_shared_info.get_wc_info();
    u32_t wc_version;
    u32_t wc_sec;
    do {
	wc_version = wc_info->wc_version;
	wc_sec = wc_info->wc_sec;
    } while( (wc_version & 1) | (wc_info->wc_version ^ wc_version) );

    return wc_sec + (xen_shared_info.get_current_time_ns() / 1000000000);
}
#endif


pgent_t *backend_resolve_addr(word_t addr, word_t &kernel_vaddr)
{
    pgent_t *pgent_ptr = xen_memory.get_pgent_ptr(addr);

    if( pgent_ptr ) {
	word_t guest_phys = xen_memory.m2p( pgent_ptr->get_address() );
	kernel_vaddr = xen_memory.guest_p2v(guest_phys) + (addr & ~PAGE_MASK);
    }

    return pgent_ptr;
}


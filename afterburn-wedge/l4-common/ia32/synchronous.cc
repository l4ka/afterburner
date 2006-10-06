/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/ia32/synchronous.cc
 * Description:   CPU emulation executed synchronously to the
 *                instruction stream.  It performs some L4-specifics.
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
#include INC_ARCH(wedge_syscalls.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)

#include <memory.h>
#include <templates.h>
#include <burn_counters.h>
#include <profile.h>

/*

Paging algorithms:

- When updating kernel mappings, use L4_Flush().
- When updating user mappings, use L4_Unmap().

- When testing and clearing kernel access bits, use L4_Flush().
- When testing and clearing user access bits, use L4_Unmap().

Problems:
- We can't get accurate page use bits for user's shared pages.  When testing
  and reseting the page bits for one user space, we reset the bits for
  all user spaces; but we have accurate bits ORed across all user spaces,
  and Linux should be storing the information in its page_map data
  structure, associated with the physical page.
- When Linux test-and-resets bits for kernel pages, L4 applies the
  operation recursively to user programs, which prevents us from detecting
  whether a user program accessed the pages.  Does Linux test-and-reset
  bits on kernel pgents?  If so, does Linux store the results within
  the page_map, so that it doesn't matter that we have already cleared
  the user entries?
- If Linux is doing a test-and-reset on both user and kernel pgents, then
  the user operations are redundant.  We can eliminate them.  The problem
  is that Linux applies test-and-reset to virtual addresses, while we
  apply it to "physical" addresses.

Answers:
- Linux seems to extract dirty bits from pgents, and to store them in
  the page_map.  See filemap_sync_pte() in mm/msync.c.
- Linux seems to tally referenced bits from all virtual address aliases: see
  mm/rmap.c.  But Linux ignores the tally; it only cares whether the
  physical page has been referenced: see mm/vmscan.c

- Linux calls filemap_sync() from msync_interval() to copy all dirty bits
  into the page_map.
*/

static const bool debug_copy_fault=0;
static const bool debug_user_pfault=0;
static const bool debug_user_except=0;
static const bool debug_user_int=0;
static const bool debug_kernel_sync_vector=0;
static const bool debug_superpages=0;
static const bool debug_device=0;

__asm__ ("					\n\
	.section .text.user, \"ax\"		\n\
	.balign	4096				\n\
afterburner_user_startup:			\n\
	movl	%gs:0, %eax			\n\
	movl	-48(%eax), %ebx			\n\
	movl	%ebx, -44(%eax)			\n\
afterburner_user_force_except:			\n\
	int	$0x0				\n\
	.previous				\n\
");

extern word_t afterburner_user_startup[];
word_t afterburner_user_start_addr = (word_t)afterburner_user_startup;


static L4_Word_t unmap_device_mem( L4_Word_t addr, L4_Word_t bits, L4_Word_t rwx=L4_FullyAccessible)
{
    L4_Word_t ret = 0;
    if (debug_device)
	con << "unmap device_mem " << (void*) addr << ", bits " << bits << "\n";;		
    
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

class unmap_cache_t
{
private:
    static const L4_Word_t cache_size = 60;
    L4_Fpage_t fpages[cache_size];
    L4_Word_t count;
    bool flush;

public:
    void commit();

    void add_mapping( L4_Word_t addr, L4_Word_t bits, L4_Word_t rwx=L4_FullyAccessible, bool do_flush=false )
	{
	    /* Check if device memory */
	    vcpu_t vcpu = get_vcpu();
	    if (contains_device_mem(addr - vcpu.get_kernel_vaddr(),
				    addr - vcpu.get_kernel_vaddr() + (1UL << bits) - 1))
		//jsXXX: virtual APIC/IO-APIC is detected as device memory
		unmap_device_mem(addr - vcpu.get_kernel_vaddr(), bits, rwx);
	    else {
		flush |= do_flush;
		if( count == cache_size )
		    commit();
		fpages[count++] = L4_FpageLog2( addr, bits ) + rwx;
	    }
	}

    unmap_cache_t()
	{ count = 0; flush = false; }
};

void unmap_cache_t::commit()
{
    if( !this->count )
	return;

    if( flush )
	L4_FlushFpages( this->count, this->fpages );
    else
	L4_UnmapFpages( this->count, this->fpages );
    this->count = 0;
    this->flush = false;
}


static unmap_cache_t unmap_cache;

static bool sync_deliver_page_not_present( 
	L4_Word_t fault_addr, L4_Word_t fault_rwx, bool user );
static bool sync_deliver_page_permissions_fault( 
	L4_Word_t fault_addr, L4_Word_t fault_rwx, bool user);



static void NORETURN
deliver_ia32_user_vector( cpu_t &cpu, L4_Word_t vector, 
	bool use_error_code, L4_Word_t error_code, L4_Word_t ip )
{
    ASSERT( vector <= cpu.idtr.get_total_gates() );
    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    flags_t old_flags = cpu.flags;
    old_flags.x.fields.fi = 1;  // Interrupts are usually enabled at user.
    cpu.flags.prepare_for_gate( gate );

    tss_t *tss = cpu.get_tss();
    if( debug_user_pfault )
	con << "tss esp0 " << (void *)tss->esp0
	    << ", tss ss0 " << tss->ss0 << '\n';

    u16_t old_cs = cpu.cs;
    u16_t old_ss = cpu.ss;
    cpu.cs = gate.get_segment_selector();
    cpu.ss = tss->ss0;

    if( gate.is_trap() )
	cpu.restore_interrupts( true );

    word_t *stack = (word_t *)tss->esp0;
    *(--stack) = old_ss;	// User ss
    *(--stack) = 0;		// User sp
    *(--stack) = old_flags.get_raw();	// User flags
    *(--stack) = old_cs;	// User cs
    *(--stack) = ip;		// User ip
    if( use_error_code )
	*(--stack) = error_code;

    __asm__ __volatile__ (
	    "movl	%0, %%esp ;"	// Switch stack
	    "jmp	*%1 ;"		// Activate gate
	    : : "r"(stack), "r"(gate.get_offset())
	    );

    panic();
}

static void NORETURN
deliver_ia32_user_vector( word_t vector, thread_info_t *thread_info )
{
    cpu_t &cpu = get_cpu();
    tss_t *tss = cpu.get_tss();

    ASSERT( vector <= cpu.idtr.get_total_gates() );
    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    flags_t old_flags = cpu.flags;
    old_flags.x.fields.fi = 1;  // Interrupts are usually enabled at user.
    cpu.flags.prepare_for_gate( gate );
    // Note: we leave interrupts disabled.

    u16_t old_cs = cpu.cs;
    u16_t old_ss = cpu.ss;
    cpu.cs = gate.get_segment_selector();
    cpu.ss = tss->ss0;

    thread_info->mr_save.exc_msg.eflags &= flags_user_mask;
    thread_info->mr_save.exc_msg.eflags |= (old_flags.x.raw & ~flags_user_mask);

#if defined(CONFIG_DO_UTCB_EXCEPTION)
#error yoda
    word_t flags_info = gate.is_trap() ? (word_t)&cpu.flags.x.raw : 0;

    // Don't restore interrupts until *after* we've read the message registers.

    L4_LoadMR( 1, thread_info->mr_save.exc_msg.eip );
    __asm__ __volatile__ (
            "movl       %4, %%esp ;"    // Switch stack
	    "movl	%%eax, -32(%%esp) ;" // Temp storage (safe since interrupts are still disabled)
            "pushl      %3 ;"           // User ss
            "movl       %%gs:0, %%eax ;"        // UTCB pointer
            "pushl      (8*4)(%%eax) ;" // User sp
            "pushl      (2*4)(%%eax) ;" // Flags
            "pushl      %1 ;"           // CS
            "pushl      (1*4)(%%eax) ;" // User ip
            "pushl      %2 ;"           // Activation point
            "movl       (5*4)(%%eax), %%edi ;"
            "movl       (6*4)(%%eax), %%esi ;"
            "movl       (7*4)(%%eax), %%ebp ;"
            "movl       (9*4)(%%eax), %%ebx ;"
            "movl       (10*4)(%%eax), %%edx ;"
            "movl       (11*4)(%%eax), %%ecx ;"
            "movl       (12*4)(%%eax), %%eax ;"

	    // Enable interrupts if necessary.
	    "pushl	%%eax ;"
	    "movl	-4(%%esp), %%eax ;"
	    "testl	%%eax, %%eax ; "
	    "jz		1f ;"
	    "bts	$9, 0(%%eax) ;"	// Set the interrupt flag.
	    "1:"
	    "popl	%%eax ;"

            "ret ;"     // Activate gate
            :
            : "A"(flags_info), "r"((u32_t)old_cs), 
              "r"(gate.get_offset()), "r"((u32_t)old_ss), "r"(tss->esp0) );
#else
    if( gate.is_trap() )
	cpu.restore_interrupts( true );

    __asm__ __volatile__ (
	    "movl	%4, %%esp ;"	// Switch stack
	    "pushl	%3 ;"		// User ss
	    "pushl	(7*4)(%%eax) ;"	// User sp
	    "pushl	(1*4)(%%eax) ;"	// Flags
	    "pushl	%1 ;"		// CS
	    "pushl	(0*4)(%%eax) ;"	// User ip
	    "pushl	%2 ;"		// Activation point
	    "movl	(4*4)(%%eax), %%edi ;"
	    "movl	(5*4)(%%eax), %%esi ;"
	    "movl	(6*4)(%%eax), %%ebp ;"
	    "movl	(8*4)(%%eax), %%ebx ;"
	    "movl	(9*4)(%%eax), %%edx ;"
	    "movl	(10*4)(%%eax), %%ecx ;"
	    "movl	(11*4)(%%eax), %%eax ;"
	    "ret ;"	// Activate gate
	    :
	    : "A"(&thread_info->mr_save.exc_msg.eip), "r"((u32_t)old_cs), 
	      "r"(gate.get_offset()), "r"((u32_t)old_ss), "r"(tss->esp0) );
#endif

    panic();
}

void NORETURN
backend_handle_user_vector( word_t vector )
{
    deliver_ia32_user_vector( get_cpu(), vector, false, 0, 0 );
}

pgent_t *
backend_resolve_addr( word_t user_vaddr, word_t &kernel_vaddr )
{
    vcpu_t &vcpu = get_vcpu();
    word_t kernel_start = vcpu.get_kernel_vaddr();

    L4_Word_t wedge_addr = vcpu.get_wedge_vaddr();
    L4_Word_t wedge_end_addr = vcpu.get_wedge_end_vaddr();
    
    static pgent_t wedge_pgent;
    if( (user_vaddr >= wedge_addr) && (user_vaddr < wedge_end_addr) )
    {
	wedge_pgent.set_address(user_vaddr 
		- vcpu.get_wedge_vaddr() 
		+ vcpu.get_wedge_paddr());
	return &wedge_pgent;
    }


    
    pgent_t *pdir = (pgent_t *)(vcpu.cpu.cr3.get_pdir_addr() + kernel_start);
    pdir = &pdir[ pgent_t::get_pdir_idx(user_vaddr) ];
    if( !pdir->is_valid() )
	return NULL;
    if( EXPECT_FALSE(pdir->is_superpage()) ) {
	kernel_vaddr = (pdir->get_address() & SUPERPAGE_MASK)
	    + (user_vaddr & ~SUPERPAGE_MASK) + kernel_start;
	return pdir;
    }

    pgent_t *ptab = (pgent_t *)(pdir->get_address() + kernel_start);
    pgent_t *pgent = &ptab[ pgent_t::get_ptab_idx(user_vaddr) ];
    if( !pgent->is_valid() )
	return NULL;
    kernel_vaddr = pgent->get_address() + (user_vaddr & ~PAGE_MASK) 
	+ kernel_start;

    return pgent;
}

void NORETURN
deliver_ia32_wedge_syscall( thread_info_t *thread_info )
{
    cpu_t &cpu = get_cpu();

    thread_info->mr_save.exc_msg.eflags &= flags_user_mask;
    thread_info->mr_save.exc_msg.eflags |= (cpu.flags.x.raw & ~flags_user_mask);

    word_t *stack = (word_t *)cpu.get_tss()->esp0;
    *(--stack) = cpu.ss;	// User ss
    *(--stack) = thread_info->mr_save.exc_msg.esp;	// User sp
    *(--stack) = thread_info->mr_save.exc_msg.eflags;	// User flags
    *(--stack) = cpu.cs;	// User cs
    *(--stack) = thread_info->mr_save.exc_msg.eip;	// User ip

    __asm__ __volatile__ (
	    "movl	%1, %%esp ;"	// Switch stack
	    "movl	(4*4)(%%eax), %%edi ;"
	    "movl	(5*4)(%%eax), %%esi ;"
	    "movl	(6*4)(%%eax), %%ebp ;"
	    "movl	(8*4)(%%eax), %%ebx ;"
	    "movl	(9*4)(%%eax), %%edx ;"
	    "movl	(10*4)(%%eax), %%ecx ;"
	    "movl	(11*4)(%%eax), %%eax ;"

	    "cmp	%%eax, max_wedge_syscall ;"
	    "jle	1f ;" // Invalid system call number
	    "pushl	wedge_syscall_table(,%%eax,4) ;" // Load syscall func.
	    
	    "movl	%%esp, %%eax ;" // Param #1: pointer to iret frame.
	    "addl	$4, %%eax ;"    // Adjust param #1.

	    // Parameter #2 is in edx, parameter #3 is in ecx.
	    "pushl	%%edi ;"	// Potential parameter, #6
	    "pushl	%%esi ;"	// Potential parameter, #5
	    "pushl	%%ebx ;"	// Potential parameter, #4

	    "call	*12(%%esp) ;"

	    "pop	%%ebx ;"	// Restore parameter.
	    "pop	%%esi ;"	// Restore parameter.
	    "pop	%%edi ;"	// Restore parameter.
	    "addl	$4, %%esp ;"	// Remove the syscall pointer.

	    "jmp burn_iret ;"		// iret to user

	    "1: mov	$2, %%eax ;"	// Illegal system call
	    "jmp burn_iret ;"		// iret to user
	    :
	    : "a"(&thread_info->mr_save.exc_msg.eip), "r"(stack)
	    );

    panic();
}


void NORETURN
backend_handle_user_exception( thread_info_t *thread_info )
{
    if( debug_user_except )
	con << "User exception at ip " 
	    << (void *)thread_info->mr_save.exc_msg.eip
	    << ", sp " << (void *)thread_info->mr_save.exc_msg.esp << '\n';

    word_t instr_addr;
    pgent_t *pgent;
    pgent = backend_resolve_addr( thread_info->mr_save.exc_msg.eip, instr_addr);
    if( !pgent )
	sync_deliver_page_not_present( instr_addr, 3 /*rwx*/, true );
    else if( pgent->is_kernel() )
	sync_deliver_page_permissions_fault( instr_addr, 3 /*rwx*/, true );

    u8_t *instr = (u8_t *)instr_addr;
    if( instr[0] == 0xcd && instr[1] >= 32 )
    {
	if( debug_user_int ) {
#if 0
	    if( thread_info->mr_save.exc_msg.eax == 3 )
		con << "> read " << thread_info->mr_save.exc_msg.ebx
		    << " " << (void *)thread_info->mr_save.exc_msg.ecx
		    << " " << thread_info->mr_save.exc_msg.edx << '\n';
	    else if( thread_info->mr_save.exc_msg.eax == 4 )
		con << "> write " << thread_info->mr_save.exc_msg.ebx
		    << " " << (void *)thread_info->mr_save.exc_msg.ecx
		    << " " << thread_info->mr_save.exc_msg.edx << '\n';
	    else if( thread_info->mr_save.exc_msg.eax == 5 )
		con << "> open " << (void *)thread_info->mr_save.exc_msg.ebx
		    << '\n';
	    else if( thread_info->mr_save.exc_msg.eax == 90 ) {
		word_t *args = (word_t *)thread_info->mr_save.exc_msg.ebx;
		con << "> mmap fd " << args[4] << ", len " << args[1]
		    << ", addr " << args[0] << ", offset " << args[5] << '\n';
	    }
	    else if( thread_info->mr_save.exc_msg.eax == 91 )
		L4_KDB_Enter("munmap");
	    else  if( thread_info->mr_save.exc_msg.eax == 2 ) {
		con << "> fork\n";
	    }
	    else if( thread_info->mr_save.exc_msg.eax == 120 ) {
		con << "> clone\n";
	    }
#endif
#if 0
	    //else
	    con << "syscall " << (void *)thread_info->mr_save.exc_msg.eax
		<< ", ebx " << (void *)thread_info->mr_save.exc_msg.ebx
		<< ", ecx " << (void *)thread_info->mr_save.exc_msg.ecx
		<< ", edx " << (void *)thread_info->mr_save.exc_msg.edx << '\n';
#endif
	}
	thread_info->mr_save.exc_msg.eip += 2; // next instruction
	if( instr[1] == 0x69 )
	    deliver_ia32_wedge_syscall( thread_info );
	else
	    deliver_ia32_user_vector( instr[1], thread_info );
    }
    else
	con << "Unsupported exception from user-level"
	    << ", user ip " << (void *)thread_info->mr_save.exc_msg.eip
	    << ", TID " << thread_info->get_tid() << '\n';

    panic();
}

bool 
backend_handle_user_pagefault(
	word_t page_dir_paddr,
	word_t fault_addr, word_t fault_ip, word_t fault_rwx,
	word_t & map_addr, word_t & map_bits, word_t & map_rwx )
{
    vcpu_t &vcpu = get_vcpu();
    cpu_t &cpu = vcpu.cpu;
    L4_Word_t link_addr = vcpu.get_kernel_vaddr();

    map_rwx = 7;

    pgent_t *pdir = (pgent_t *)(page_dir_paddr + link_addr);
    pdir = &pdir[ pgent_t::get_pdir_idx(fault_addr) ];
    if( !pdir->is_valid() ) {
	if( debug_user_pfault )
	    con << "user pdir not present\n";
	goto not_present;
    }

    if( pdir->is_superpage() ) {
	map_addr = (pdir->get_address() & SUPERPAGE_MASK) + 
	    (fault_addr & ~SUPERPAGE_MASK);
	if( !pdir->is_writable() )
	    map_rwx = 5;
	map_bits = SUPERPAGE_BITS;
	if( debug_superpages )
	    con << "user super page\n";
    }
    else 
    {
	pgent_t *ptab = (pgent_t *)(pdir->get_address() + link_addr);
	pgent_t *pgent = &ptab[ pgent_t::get_ptab_idx(fault_addr) ];
	if( !pgent->is_valid() )
	    goto not_present;
	map_addr = pgent->get_address() + (fault_addr & ~PAGE_MASK);
	if( !pgent->is_writable() )
	    map_rwx = 5;
	map_bits = PAGE_BITS;
    }

    // TODO: figure out whether the mapping given to user actuall exists
    // in kernel space.  If not, we have to grant the page.
    if( map_addr < link_addr )
	map_addr += link_addr;

    if( (map_rwx & fault_rwx) != fault_rwx )
	goto permissions_fault;

    // TODO: only do this if the mapping exists in kernel space too.
    *(volatile word_t *)map_addr;
    return true;

not_present:
    if( page_dir_paddr != cpu.cr3.get_pdir_addr() )
	return false;	// We have to delay fault delivery.
    cpu.cr2 = fault_addr;
    if( debug_user_pfault )
	con << "page not present, fault addr " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip << '\n';
    deliver_ia32_user_vector( cpu, 14, true, 4 | ((fault_rwx & 2) | 0), fault_ip );
    goto unhandled;

permissions_fault:
    if( page_dir_paddr != cpu.cr3.get_pdir_addr() )
	return false;	// We have to delay fault delivery.
    cpu.cr2 = fault_addr;
    if( debug_user_pfault )
	con << "Delivering page fault for addr " << (void *)fault_addr
	    << ", permissions " << fault_rwx 
	    << ", ip " << (void *)fault_ip << '\n';
    deliver_ia32_user_vector( cpu, 14, true, 4 | ((fault_rwx & 2) | 1), fault_ip );
    goto unhandled;

unhandled:
    con << "Unhandled page permissions fault, fault addr " << (void *)fault_addr
	<< ", ip " << (void *)fault_ip << ", fault rwx " << fault_rwx << '\n';
    panic();
    return false;
}

void bind_guest_uaccess_fault_handler( void )
{
    extern char uaccess_fault_fixup[], uaccess_fault_addr[];

    ASSERT( guest_uaccess_fault );
    guest_uaccess_fault->faulting_instr_ip = (word_t)uaccess_fault_addr;
    guest_uaccess_fault->fixup_ip = (word_t)uaccess_fault_fixup;
}

bool backend_sync_deliver_vector( L4_Word_t vector, bool old_int_state, bool use_error_code, L4_Word_t error_code )
    // Must be called with interrupts disabled.
{
    cpu_t &cpu = get_cpu();
    ASSERT(!cpu.interrupts_enabled());

    if( vector > cpu.idtr.get_total_gates() ) {
	con << "No IDT entry for vector " << vector << '\n';
	return false;
    }

    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    if( debug_kernel_sync_vector )
	con << "Delivering vector " << vector
	    << ", handler ip " << (void *)gate.get_offset() << '\n';

    flags_t old_flags = cpu.flags;
    old_flags.x.fields.fi = old_int_state;
    cpu.flags.prepare_for_gate( gate );

    u16_t old_cs = cpu.cs;
    cpu.cs = gate.get_segment_selector();

    bool result = true;
    __asm__ __volatile__ (
	    "pushl	%1 ;"
	    "pushl	%2 ;"
	    "pushl	$uaccess_fault_addr ;"
	    "test	%%al, %%al ;"
	    "jz		1f ;"
	    "pushl	%4 ;"
	    "mov	$1, %0 ;"	// Success result code
	    "1: jmp	*%3 ;"

	    ".globl uaccess_fault_fixup ; "
	    "uaccess_fault_fixup: "
	    "mov	$0, %0 ;"	// Failure result code

	    ".globl uaccess_fault_addr ; "
	    "uaccess_fault_addr: "
	    : "=a"(result)
	    : "r"(old_flags.x.raw), "r"((u32_t)old_cs),
	      "r"(gate.get_offset()), "r"(error_code), "0"(use_error_code)
	    : "flags", "memory" );

    if( debug_kernel_sync_vector )
	con << "Finished synchronous vector delivery.\n";

    return result;
}

static bool
sync_deliver_page_not_present( L4_Word_t fault_addr, L4_Word_t fault_rwx, bool user )
{
    cpu_t &cpu = get_cpu();
    bool int_save = cpu.disable_interrupts();
    cpu.cr2 = fault_addr;
    return backend_sync_deliver_vector( 14, int_save, 
	    true, (user << 2) | ((fault_rwx & 2) | 0) );
}

static bool
sync_deliver_page_permissions_fault( L4_Word_t fault_addr, L4_Word_t fault_rwx, bool user )
{
    cpu_t &cpu = get_cpu();
    bool int_save = cpu.disable_interrupts();
    cpu.cr2 = fault_addr;
    return backend_sync_deliver_vector( 14, int_save, 
	    true, (user << 2) | ((fault_rwx & 2) | 1) );
}


INLINE u32_t word_text( char a, char b, char c, char d )
{
    return (a)<<0 | (b)<<8 | (c)<<16 | (d)<<24;
}


void backend_cpuid_override( 
	u32_t func, u32_t max_basic, u32_t max_extended,
	frame_t *regs )
{
    if( (func > max_basic) && (func < 0x80000000) )
	func = max_basic;
    if( func > max_extended )
	func = max_extended;

    if( func <= max_basic )
    {
	switch( func )
	{
	    case 1:
		bit_clear( 3, regs->x.fields.ecx ); // Disable monitor/mwait.
		break;
	}
    }
    else {
	switch( func )
	{
	    case 0x80000002:
		regs->x.fields.eax = word_text('L','4','K','a');
		regs->x.fields.ebx = word_text(':',':','P','i');
		regs->x.fields.ecx = word_text('s','t','a','c');
		regs->x.fields.edx = word_text('h','i','o',' ');
		break;
	    case 0x80000003:
		regs->x.fields.eax = word_text('(','h','t','t');
		regs->x.fields.ebx = word_text('p',':','/','/');
		regs->x.fields.ecx = word_text('l','4','k','a');
		regs->x.fields.edx = word_text('.','o','r','g');
		break;
	    case 0x80000004:
		regs->x.fields.eax = word_text(')',0,0,0);
		break;
	}
    }
}

static word_t
flush_page( vcpu_t &vcpu, word_t addr, word_t bits, word_t permissions=L4_FullyAccessible )
{
    word_t ret;
    
    if (contains_device_mem(addr - vcpu.get_kernel_vaddr(),
			    addr - vcpu.get_kernel_vaddr() + (1UL << bits) - 1))
	ret = unmap_device_mem(addr - vcpu.get_kernel_vaddr(), bits, permissions);
    else
        ret = L4_Rights(L4_Flush( L4_FpageLog2(addr, bits) + permissions ));
    
    return ret;
}

static word_t
unmap_page( vcpu_t &vcpu, word_t addr, word_t bits, word_t permissions )
{
    word_t ret;
    
    if (contains_device_mem(addr - vcpu.get_kernel_vaddr(),
			    addr - vcpu.get_kernel_vaddr() + (1UL << bits) - 1))
	ret = unmap_device_mem(addr - vcpu.get_kernel_vaddr(), bits, permissions);
    else
        ret = L4_Rights(L4_Unmap( L4_FpageLog2(addr, bits) + permissions ));
    
    return ret;
}

#if 0
static void
flush_ptab( vcpu_t &vcpu, pgent_t *ptab )
{
    word_t old_mapping;
    for( word_t idx = 0; idx < (PAGE_SIZE/sizeof(word_t)); idx++ )
    {
	pgent_t &ptab_entry = ptab[idx];
	if( !ptab_entry.is_valid() )
	    continue;
	old_mapping = ptab_entry.get_address() + vcpu.get_kernel_vaddr();
	flush_page( vcpu, old_mapping, PAGE_BITS );
    }
}
#endif

static void
unmap_ptab( vcpu_t &vcpu, pgent_t *pdir_ent, word_t pdir_idx )
{
    pgent_t *ptab = (pgent_t *)
	(pdir_ent->get_address() + vcpu.get_kernel_vaddr());

    // If a kernel pgtab, then we must flush from the kernel's address space.
    bool flush = pdir_idx >= pgent_t::get_pdir_idx(vcpu.get_kernel_vaddr());

    word_t old_mapping;
    for( word_t idx = 0; idx < (PAGE_SIZE/sizeof(word_t)); idx++ )
    {
	pgent_t &ptab_entry = ptab[idx];
	if( !ptab_entry.is_valid() )
	    continue;
	old_mapping = ptab_entry.get_address() + vcpu.get_kernel_vaddr();
	unmap_cache.add_mapping( old_mapping, PAGE_BITS, L4_FullyAccessible,
		flush );
    }
}

extern "C" word_t __attribute__((regparm(1)))
backend_pte_read_patch( pgent_t *pgent )
{
    ON_INSTR_PROFILE( 
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    return pgent->get_raw();
}

extern "C" word_t __attribute__((regparm(1)))
backend_pgd_read_patch( pgent_t *pgent )
{
    ON_INSTR_PROFILE( 
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    return pgent->get_raw();
}

extern "C" word_t __attribute__((regparm(2)))
backend_pte_xchg_patch( pgent_t new_val, pgent_t *pgent )
{
    ON_INSTR_PROFILE( 
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    pgent_t old_pgent = *pgent; // Make a copy.
    if( new_val.get_raw() != 0 )
	panic();
    pgent->clear();		// Remove from page table.

    if( !old_pgent.is_valid() )
	return old_pgent.x.raw;

    // Perform the unmap + status bit check.
    vcpu_t &vcpu = get_vcpu();
    word_t mapping = old_pgent.get_address() + vcpu.get_kernel_vaddr();

    word_t rwx = unmap_page( vcpu, mapping, PAGE_BITS, L4_FullyAccessible );

    // Update the status bits in the copy.
    if( rwx & L4_Writable )
	old_pgent.set_dirty(1);
    if( rwx & (L4_Readable | L4_eXecutable) )
	old_pgent.set_accessed(1);

    return old_pgent.x.raw;
}


extern "C" void __attribute__((regparm(2)))
backend_pte_write_patch( pgent_t new_val, pgent_t *old_pgent )
{
    ON_INSTR_PROFILE( 
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    if( old_pgent->is_valid() )
    {
	word_t rights = L4_NoAccess;
	if( old_pgent->get_address() == new_val.get_address() ) {
	    if( !new_val.is_readable() && old_pgent->is_readable() )
		rights |= L4_Readable | L4_eXecutable;
	    if( !new_val.is_writable() && old_pgent->is_writable() )
		rights |= L4_Writable;
	}
	else
	    rights = L4_FullyAccessible;

	if( rights ) {
	    word_t old_mapping = old_pgent->get_address() + 
		get_vcpu().get_kernel_vaddr();

	    word_t rwx = unmap_page( get_vcpu(), old_mapping, PAGE_BITS, rights );
	    if( old_pgent->get_address() == new_val.get_address() ) {
		// Update the status bits in the copy.
		if( rwx & L4_Writable )
		    new_val.set_dirty(1);
		if( rwx & (L4_Readable | L4_eXecutable) )
		    new_val.set_accessed(1);
	    }
	}
    }

    *old_pgent = new_val;
}

extern "C" void __attribute__((regparm(2)))
backend_pte_or_patch( word_t bits, pgent_t *pgent )
{
    PANIC( "unimplemented" );
}

extern "C" void __attribute__((regparm(2)))
backend_pte_and_patch( word_t bits, pgent_t *pgent )
{
    PANIC( "unimplemented" );
}


extern "C" void __attribute__((regparm(2)))
backend_pgd_write_patch( pgent_t new_val, pgent_t *old_pgent )
{
    ON_INSTR_PROFILE( 
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    vcpu_t &vcpu = get_vcpu();
    word_t old_mapping;

    if( old_pgent->is_valid() ) {
	word_t idx = (word_t(old_pgent) & ~PAGE_MASK) / sizeof(pgent_t);

	if( old_pgent->is_superpage() ) {
	    word_t rights = old_pgent->get_address() == new_val.get_address() ? L4_NoAccess : L4_FullyAccessible;
	    old_mapping = old_pgent->get_address() + vcpu.get_kernel_vaddr();
	    unmap_cache.add_mapping( old_mapping, SUPERPAGE_BITS, rights, 
		    idx >= pgent_t::get_pdir_idx(vcpu.get_kernel_vaddr()) );
	    if( debug_superpages )
		con << "flush super page " << (void *) old_pgent->get_raw() << "\n";
	}
	else {
	    unmap_ptab( vcpu, old_pgent, idx );
	}
    }

    *old_pgent = new_val;
}

#define _PAGE_BIT_RW		1
#define _PAGE_BIT_ACCESSED	5
#define _PAGE_BIT_DIRTY		6

extern "C" word_t __attribute__((regparm(2)))
backend_pte_test_clear_patch( word_t bit, pgent_t *pgent )
{
    ON_INSTR_PROFILE( 
	    instr_profile_return_addr( (word_t)__builtin_return_address(0) ));

    int ret = 0;
    if( !pgent->is_valid() )
	return 0;

    vcpu_t &vcpu = get_vcpu();
    word_t mapping = pgent->get_address() + vcpu.get_kernel_vaddr();

    word_t rwx;
    if (bit == _PAGE_BIT_RW)
	rwx = unmap_page( vcpu, mapping, PAGE_BITS, L4_Writable );
    else
	// Assuming a 4KB page entry.
	rwx = unmap_page( vcpu, mapping, PAGE_BITS, L4_NoAccess );
    
    if( rwx & L4_Writable )
	pgent->set_dirty(1);
    if( rwx & (L4_Readable | L4_eXecutable) )
	pgent->set_accessed(1);
    
    switch (bit) {
    case _PAGE_BIT_RW:
	ret = pgent->is_writable();
	pgent->set_read_only();
	break;
    case _PAGE_BIT_DIRTY:
	ret = pgent->is_dirty();
	pgent->set_dirty(0);
	break;
    case _PAGE_BIT_ACCESSED:
	ret = pgent->is_accessed();
	pgent->set_accessed(0);
	break;
    default:
	con << "unhandled access bit cleared: " << bit << "\n";
    }
    return ret;
}


#if 1
static void flush_old_ptab( vcpu_t &vcpu, pgent_t *new_ptab, pgent_t *old_ptab )
{
    for( word_t idx = 0; idx < (PAGE_SIZE/sizeof(word_t)); idx++ )
    {
	pgent_t &old_pgent = old_ptab[idx];

	if( !old_pgent.is_valid() || old_pgent.is_global() )
	    continue;

    	word_t mapping = old_pgent.get_address() + vcpu.get_kernel_vaddr();
	unmap_cache.add_mapping( mapping, PAGE_BITS );
    }
}
#endif

DECLARE_BURN_COUNTER(manual_cr3_flush);

void backend_flush_old_pdir( u32_t new_pdir_paddr, u32_t old_pdir_paddr )
{
    vcpu_t &vcpu = get_vcpu();
    bool page_global_enabled = vcpu.cpu.cr4.is_page_global_enabled() && !vcpu.is_global_crap();

    if( page_global_enabled && vcpu.vaddr_global_only ) {
	// The kernel added only global pages since the last flush.  Thus
	// we don't have anything new to flush.
	vcpu.vaddr_stats_reset();
	unmap_cache.commit();
	return;
    }
    else
	INC_BURN_COUNTER(manual_cr3_flush);

    pgent_t *new_pdir = (pgent_t *)(new_pdir_paddr + vcpu.get_kernel_vaddr());
    pgent_t *old_pdir = (pgent_t *)(old_pdir_paddr + vcpu.get_kernel_vaddr());

    for( word_t idx = vcpu.vaddr_flush_min >> PAGEDIR_BITS;
	    idx <= vcpu.vaddr_flush_max >> PAGEDIR_BITS; idx++ )
    {
	pgent_t &new_pgent = new_pdir[idx];
	pgent_t &old_pgent = old_pdir[idx];

	if( !old_pgent.is_valid() )
	    continue;	// No mapping to flush.
	if( old_pgent.is_superpage() )
	{
	    if( page_global_enabled && old_pgent.is_global() )
		continue;
	    word_t mapping = old_pgent.get_address() + vcpu.get_kernel_vaddr();
    	    if( debug_superpages )
		con << "flush super page " << (void *) old_pgent.get_raw() << "\n";
	    unmap_cache.add_mapping( mapping, SUPERPAGE_BITS );
	}
	else if( page_global_enabled
		&& old_pgent.get_address() == new_pgent.get_address()
		&& old_pgent.is_writable() == new_pgent.is_writable() )
	{
	    pgent_t *new_ptab = (pgent_t *)(new_pgent.get_address() + vcpu.get_kernel_vaddr());
	    pgent_t *old_ptab = (pgent_t *)(old_pgent.get_address() + vcpu.get_kernel_vaddr());
	    flush_old_ptab( vcpu, new_ptab, old_ptab );
	}
	else if( old_pgent.get_address() != old_pdir_paddr ) {
	    // Note: FreeBSD installs recursive entries, i.e., a pdir
	    // entry that points back at itself, thus establishing the
	    // pdir as a page table.  Skip recursive entries.
	    unmap_ptab( vcpu, &old_pgent, idx );
	}
    }

    vcpu.vaddr_stats_reset();
    unmap_cache.commit();
}

extern void backend_flush_vaddr( word_t vaddr )
    // Flush the mapping of an arbitrary virtual address.  The mapping 
    // could be of any size, so we must walk the page table to determine
    // the page size.
{
    vcpu_t &vcpu = get_vcpu();
    word_t kernel_start = vcpu.get_kernel_vaddr();

    pgent_t *pdir = (pgent_t *)(vcpu.cpu.cr3.get_pdir_addr() + kernel_start);
    pdir = &pdir[ pgent_t::get_pdir_idx(vaddr) ];
    if( !pdir->is_valid() )
	return;
    if( pdir->is_superpage() ) {
	vaddr = pdir->get_address() + kernel_start;
	unmap_cache.add_mapping( vaddr, SUPERPAGE_BITS );
	unmap_cache.commit();
	return;
    }

    pgent_t *ptab = (pgent_t *)(pdir->get_address() + kernel_start);
    pgent_t *pgent = &ptab[ pgent_t::get_ptab_idx(vaddr) ];
    if( !pgent->is_valid() )
	return;
    vaddr = pgent->get_address() + kernel_start;
    unmap_cache.add_mapping( vaddr, PAGE_BITS );
    unmap_cache.commit();
}


void backend_invalidate_tlb( void )
{
    vcpu_t &vcpu = get_vcpu();

    vcpu.invalidate_globals();

//    word_t pdir = vcpu.cpu.cr3.get_pdir_addr();
//    backend_flush_old_pdir( pdir, pdir );
}

#if defined(CONFIG_GUEST_UACCESS_HOOK)
DECLARE_BURN_COUNTER(get_user_hook);
DECLARE_BURN_COUNTER(put_user_hook);
DECLARE_BURN_COUNTER(copy_from_user_hook);
DECLARE_BURN_COUNTER(copy_from_user_bytes);
DECLARE_BURN_COUNTER(copy_to_user_hook);
DECLARE_BURN_COUNTER(copy_to_user_bytes);
DECLARE_BURN_COUNTER(clear_user_hook);
DECLARE_BURN_COUNTER(strnlen_user_hook);
DECLARE_BURN_COUNTER(strncpy_from_user_hook);
DECLARE_BURN_COUNTER(uaccess_resolve_fault);
DECLARE_BURN_COUNTER(uaccess_to_kernel);

INLINE pgent_t *
uaccess_resolve_addr( word_t addr, word_t rwx, word_t & kernel_vaddr )
{
    pgent_t *pgent = backend_resolve_addr( addr, kernel_vaddr );
    if( EXPECT_FALSE(!pgent || ((rwx & 2) && !pgent->is_writable())) )
    {
	INC_BURN_COUNTER(uaccess_resolve_fault);
	if( debug_copy_fault )
	    con << "uaccess fault at " << (void *)addr << '\n';
	bool result;
	if( !pgent )
	    result = sync_deliver_page_not_present( addr, rwx, false );
	else
	    result = sync_deliver_page_permissions_fault( addr, rwx, false );
	if( !result )
	    return NULL;

	pgent = backend_resolve_addr( addr, kernel_vaddr );
	if( !pgent || ((rwx & 2) && !pgent->is_writable()) ) {
	    con << "double resolve failure: " << (void*)addr << ' ' << (void*)pgent << ' ' << rwx << ' ' << (void *)__builtin_return_address(0) << '\n';
	    return NULL;
	}
    }

    if( pgent->is_kernel() )
	INC_BURN_COUNTER( uaccess_to_kernel );

    return pgent;
}

INLINE word_t
copy_to_user_page( void *to, const void *from, unsigned long n )
{
    word_t kernel_vaddr;
    pgent_t * pgent = uaccess_resolve_addr( (word_t)to, 6, kernel_vaddr );
    if( !pgent )
	return 0;

    pgent->set_accessed( true );
    pgent->set_dirty( true );

    memcpy( (void *)kernel_vaddr, from, n );
    return n;
}

INLINE word_t
copy_from_user_page( void *to, const void *from, unsigned long n )
{
    word_t kernel_vaddr;
    pgent_t * pgent = uaccess_resolve_addr( (word_t)from, 4, kernel_vaddr );
    if( !pgent )
	return 0;

    pgent->set_accessed( true );

    memcpy( to, (void *)kernel_vaddr, n );
    return n;
}

static word_t
clear_user_page( void *to, unsigned long n )
{
    word_t kernel_vaddr;
    pgent_t * pgent = uaccess_resolve_addr( (word_t)to, 6, kernel_vaddr );
    if( !pgent )
	return 0;

    pgent->set_accessed( true );
    pgent->set_dirty( true );

    unsigned long i;
    if( (kernel_vaddr % sizeof(word_t)) || (n % sizeof(word_t)) ) {
	// Unaligned stuff.
	for( i = 0; i < n; i++ )
	    ((u8_t *)kernel_vaddr)[i] = 0;
    }
    else {
	// Aligned stuff.
	for( i = 0; i < n/sizeof(word_t); i++ )
	    ((word_t *)kernel_vaddr)[i] = 0;
    }
    return n;
}

static bool
strnlen_user_page( const char *s, word_t n, word_t *len )
{
    word_t kernel_vaddr;
    pgent_t *pgent = uaccess_resolve_addr( (word_t)s, 4, kernel_vaddr );
    if( !pgent ) {
	*len = 0;     // Reset the length.
	return false; // Stop searching.
    }

    word_t res, end;
    __asm__ __volatile__ (
	    "0:	repne; scasb\n"
	    "	sete %b1\n"
	    :"=c" (res), "=a" (end)
	    :"0" (n), "1"(0), "D"(kernel_vaddr)
	    );

    pgent->set_accessed( true );

    /* End is 0 -- max number of bytes searched.
     * End is > 0 -- end of string reached, res contains the number of
     *   remaining bytes.
     */
    *len += n - res;
    if( end )
	return false; // Stop searching.
    return true;
}

static word_t 
strncpy_from_user_page( char *dst, const char *src, word_t copy_size, word_t *success, bool *finished )
{
    word_t kernel_vaddr;
    pgent_t * pgent = uaccess_resolve_addr( (word_t)src, 4, kernel_vaddr );
    if( !pgent ) {
	*finished = true;
	*success = 0;
	return 0;
    }
    

    pgent->set_accessed( true );

    word_t remain = copy_size;
    word_t d0, d1, d2;
    __asm__ __volatile__ (
	    "	testl %0,%0\n"
	    "	jz 1f\n"
	    "0:	lodsb\n"
	    "	stosb\n"
	    "	testb %%al,%%al\n"
	    "	jz 1f\n"
	    "	decl %0\n"
	    "	jnz 0b\n"
	    "1:\n"
	    : "=c"(remain), "=&a"(d0), "=&S"(d1),
	      "=&D"(d2)
	    : "0"(remain), "2"(kernel_vaddr), "3"(dst)
	    : "memory" );

    if( remain )
	*finished = true;
    return copy_size - remain;
}

word_t
backend_get_user_hook( void *to, const void *from, word_t n )
{
    INC_BURN_COUNTER(get_user_hook);

    word_t kernel_vaddr;
    pgent_t *pgent = uaccess_resolve_addr( (word_t)from, 4, kernel_vaddr );
    if( !pgent )
	return 0;

    switch( n ) {
	case 1: *(u8_t *)to = *(u8_t *)kernel_vaddr; break;
	case 2: *(u16_t *)to = *(u16_t *)kernel_vaddr; break;
	case 4: *(u32_t *)to = *(u32_t *)kernel_vaddr; break;
	case 8: *(u64_t *)to = *(u64_t *)kernel_vaddr; break;
	default:
		ASSERT( !"not supported" );
    }

    pgent->set_accessed( true );

    return n;
}

word_t backend_put_user_hook( void *to, const void *from, word_t n )
{
    INC_BURN_COUNTER(put_user_hook);

    word_t kernel_vaddr;
    pgent_t *pgent = uaccess_resolve_addr( (word_t)to, 6, kernel_vaddr );
    if( !pgent )
	return 0;

    switch( n ) {
	case 1: *(u8_t *)kernel_vaddr = *(u8_t *)from; break;
	case 2: *(u16_t *)kernel_vaddr = *(u16_t *)from; break;
	case 4: *(u32_t *)kernel_vaddr = *(u32_t *)from; break;
	case 8: *(u64_t *)kernel_vaddr = *(u64_t *)from; break;
	default:
		ASSERT( !"not supported" );
    }

    pgent->set_accessed( true );
    pgent->set_dirty( true );

    return n;
}

word_t backend_copy_from_user_hook( void *to, const void *from, word_t n )
{
    INC_BURN_COUNTER(copy_from_user_hook);
    ADD_BURN_COUNTER(copy_from_user_bytes, n);

    word_t result;
    word_t remaining = n;
    word_t copy_size = (word_t)from & ~PAGE_MASK;

    if( copy_size ) {
	copy_size = min( (word_t)PAGE_SIZE - copy_size, remaining );
	result = copy_from_user_page( to, from, copy_size );
	remaining -= result;
	if( result != copy_size )
	    return n - remaining;
    }

    while( remaining ) {
	from = (void *)(word_t(from) + copy_size);
	to = (void *)(word_t(to) + copy_size);
	copy_size = min( (word_t)PAGE_SIZE, remaining );
	result = copy_from_user_page( to, from, copy_size );
	remaining -= result;
	if( result != copy_size )
	    return n - remaining;
    }

    return n;
}

word_t backend_copy_to_user_hook( void *to, const void *from, word_t n )
{
    ADD_BURN_COUNTER(copy_to_user_bytes, n);
    INC_BURN_COUNTER(copy_to_user_hook);

    word_t result;
    word_t remaining = n;
    word_t copy_size = (word_t)to & ~PAGE_MASK;

    if( copy_size ) {
	copy_size = min( (word_t)PAGE_SIZE - copy_size, remaining );
	result = copy_to_user_page( to, from, copy_size );
	remaining -= result;
	if( result != copy_size )
	    return n - remaining;
    }

    while( remaining ) {
	from = (void *)(word_t(from) + copy_size);
	to = (void *)(word_t(to) + copy_size);
	copy_size = min( (word_t)PAGE_SIZE, remaining );
	result = copy_to_user_page( to, from, copy_size );
	remaining -= result;
	if( result != copy_size )
	    return n - remaining;
    }

    return n;
}

word_t
backend_clear_user_hook( void *dst, word_t n )
{
    INC_BURN_COUNTER(clear_user_hook);

    word_t result;
    word_t remaining = n;
    word_t clear_size = (word_t)dst & ~PAGE_MASK;

    if( clear_size ) {
	clear_size = min( (word_t)PAGE_SIZE - clear_size, remaining );
	result = clear_user_page( dst, clear_size );
	remaining -= result;
	if( result != clear_size )
	    return n - remaining;
    }

    while( remaining ) {
	dst = (void *)(word_t(dst) + clear_size );
	clear_size = min( word_t(PAGE_SIZE), remaining );
	result = clear_user_page( dst, clear_size );
	remaining -= result;
	if( result != clear_size )
	    return n - remaining;
    }

    return n;
}

word_t
backend_strnlen_user_hook( const char *s, word_t n )
    // Must include the size of the terminating NULL.
{
    INC_BURN_COUNTER(strnlen_user_hook);

    word_t search_size = PAGE_SIZE - ((word_t)s & ~PAGE_MASK );
    word_t len = 0;

    if( !search_size )
	search_size = PAGE_SIZE;
    if( search_size > n )
	search_size = n;

    while( n > 0 ) {
	if( !strnlen_user_page(s, search_size, &len) )
	    return len;

	s += search_size;
	n =- search_size;
	search_size = PAGE_SIZE;
	if( search_size > n )
	    search_size = n;
    }

    return len;
}

word_t
backend_strncpy_from_user_hook( char *dst, const char *src, word_t count, word_t *success )
{
    INC_BURN_COUNTER(strncpy_from_user_hook);

    word_t result;
    word_t remaining = count;
    word_t copy_size = (word_t)src & ~PAGE_MASK;
    bool finished = false;

    *success = 1;

    if( copy_size ) {
	copy_size = min( (word_t)PAGE_SIZE - copy_size, remaining );
	result = strncpy_from_user_page( dst, src, copy_size, success, &finished );
	remaining -= result;
	if( finished )
	    return count - remaining;
    }

    while( remaining ) {
	src += copy_size;
	dst += copy_size;
	copy_size = min( (word_t)PAGE_SIZE, remaining );
	result = strncpy_from_user_page( dst, src, copy_size, success, &finished );
	remaining -= result;
	if( finished )
	    return count - remaining;
    }

    return count;
}


#endif

#if defined(CONFIG_GUEST_MODULE_HOOK)
bool backend_module_rewrite_hook( elf_ehdr_t *ehdr )
{
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
}
#endif


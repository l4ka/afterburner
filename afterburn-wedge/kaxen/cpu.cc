/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/cpu.cc
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

#include INC_ARCH(cpu.h)

#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(cpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)

void xen_deliver_async_vector( 
	word_t vector, xen_frame_t *frame, bool use_error_code,
	bool prior_irq_flag )
{
    cpu_t &cpu = get_cpu();

    if( vector > cpu.idtr.get_total_gates() )
	PANIC( "No IDT entry for vector " << vector );

    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

#if defined(CONFIG_IA32_FAST_VECTOR)
    lret_frame_t *trampoline;
    if( use_error_code )
	trampoline = &frame->err_trampoline.lret;
    else
	trampoline = &frame->trampoline.lret;
    trampoline->ip = gate.get_offset();
    trampoline->cs = XEN_CS_KERNEL;

#if !defined(CONFIG_VMI_SUPPORT)
    ASSERT( cpu.cs == ((GUEST_CS_KERNEL_SEGMENT << 3) | 0) );
#endif
    if( gate.is_interrupt() )
	cpu.disable_interrupts();
    if( cpu_t::get_segment_privilege(frame->iret.cs) < 3 )
	frame->iret.cs = (GUEST_CS_KERNEL_SEGMENT << 3) | 0;
#else
    flags_t old_flags;
    old_flags.x.raw = 
	cpu.flags.get_system_flags() | frame->iret.flags.get_user_flags();
    old_flags.x.fields.fi |= prior_irq_flag;
    cpu.flags.prepare_for_gate( gate );

    u16_t old_cs = cpu.cs;
    u16_t old_ss = cpu.ss;
    u16_t new_cs = gate.get_segment_selector();
    bool from_user = cpu.get_privilege() != cpu_t::get_segment_privilege(new_cs);
    if( from_user )
	cpu.ss = cpu.get_tss()->ss0;
    cpu.cs = new_cs;

    // Synthesize a trampoline lret frame, preceeding the true iret frame.
    // We use lret, rather than ret, to preserve the CPU's call/ret matching.
    // Additionally, we'll eventually support pass-through segmentation.
    lret_frame_t *trampoline;
    if( use_error_code )
	trampoline = &frame->err_trampoline.lret;
    else
	trampoline = &frame->trampoline.lret;
    trampoline->ip = gate.get_offset();
    trampoline->cs = XEN_CS_KERNEL;

    // Convert the hardware iret frame into a virtual frame, inserting
    // the flags, cs, and ss that the guest OS expects.
    frame->iret.cs = old_cs;
    frame->iret.flags.x.raw = old_flags.x.raw;
    if( from_user )
	frame->iret.ss = old_ss;

    if( gate.is_trap() )
        // Traps can have interrupts enabled.
	cpu.restore_interrupts( old_flags.x.fields.fi );
#endif

    if( use_error_code ) {
	__asm__ __volatile__ (
		"movl	%0, %%esp ;"
		"popl	%%eax ;"
		"popl	%%ecx ;"
		"popl	%%edx ;"
		"popl	%%ebx ;"
		"popl	%%ebp ;"
		"popl	%%esi ;"
		"popl	%%edi ;"
		"lret ;"
		:
		: "r"(frame)
		);
    }
    else {
	__asm__ __volatile__ (
		"movl	%0, %%esp ;"
		"popl	%%eax ;"
		"popl	%%ecx ;"
		"popl	%%edx ;"
		"popl	%%ebx ;"
		"popl	%%ebp ;"
		"popl	%%esi ;"
		"popl	%%edi ;"
		"addl	$4, %%esp ;" // Skip the spacer.
		"lret ;"
		:
		: "r"(frame)
		);
    }
}

void backend_sync_deliver_vector( 
	word_t vector, bool old_int_state, 
	bool use_error_code, word_t error_code )
// Note: interrupts must be disabled.
{
    cpu_t &cpu = get_cpu();

    if( vector > cpu.idtr.get_total_gates() )
	PANIC( "No IDT entry for vector " << vector );

    gate_t *idt = cpu.idtr.get_gate_table();
    gate_t &gate = idt[ vector ];

    ASSERT( gate.is_trap() || gate.is_interrupt() );
    ASSERT( gate.is_present() );
    ASSERT( gate.is_32bit() );

    flags_t old_flags = cpu.flags;
    old_flags.x.fields.fi = old_int_state;
    cpu.flags.prepare_for_gate( gate );

    u16_t old_cs = cpu.cs;
    cpu.cs = gate.get_segment_selector();

    if( use_error_code )
    {
	__asm__ __volatile__ (
		"pushl  %0 ;"
       		"pushl  %1 ;"
       		"pushl  $1f ;"
       		"pushl  %3 ;"
       		"jmp    *%2 ;"
       		"1:"
       		:
       		: "r"(old_flags.x.raw), "r"((u32_t)old_cs),
     		  "r"(gate.get_offset()), "r"(error_code)
     		: "flags", "memory" );
    }
    else
    {
 	__asm__ __volatile__ (
 		"pushl  %0 ;"
 		"pushl  %1 ;"
 		"pushl  $1f ;"
 		"jmp    *%2 ;"
 		"1:"
 		:
 		: "r"(old_flags.x.raw), "r"((u32_t)old_cs),
		  "r"(gate.get_offset())
		: "flags", "memory" );
    }

    // We resume execution here.
}


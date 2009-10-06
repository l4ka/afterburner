/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/ia32/frontend.S
 * Description:   Assembler macros for mapping sensitive instructions to
 *                emulation code.
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
#ifndef __IA32__FRONTEND_S__
#define __IA32__FRONTEND_S__

#if defined(CONFIG_BURN_COUNTERS)

.macro burn_counter_inc name
/* Increment a burn counter.  */
	incl __counter__\name
.endm

.macro burn_counter name
/* Allocate a burn counter, obeying the definitions in burn_counters.h.
 */
.pushsection .burn_counters, "aw"
	__counter__\name:
	.long	0
	.long	__counter_string__\name
.popsection
/* Encode the burn counter's name as a string.  */
.pushsection .burn_strings, "a"
	__counter_string__\name:
	.string "\name"
.popsection
.endm

#else

.macro burn_counter_inc name
.endm

.macro burn_counter name
.endm

#endif	/* CONFIG_BURN_COUNTERS */


.macro burn_func name section
/* Define the prolog of a burn trampoline. */
.ifnes "\section", ""
.section \section, "ax"
.else
.text
.endif
.globl \name
.type \name,@function
\name:
	burn_counter \name
	burn_counter_inc \name
.endm

.macro	burn_call_c func
/* Invoke a front-end C function that expects a burn_clobbers_frame_t as
 * the parameter.
 */
	__afterburn_save_clobbers
	pushl	%esp		/* Define a C parameter that points at our
				   stack frame. */
	subl	$8, 0(%esp)	/* Adjust the parameter to include our 
				   upcoming call frame. */
	call	\func
	__afterburn_restore_clobbers 4
	ret
.endm

.macro __afterburn_save_clobbers
	pushl	%eax
	pushl	%ecx
	pushl	%edx
.endm

.macro __afterburn_restore_clobbers drop
.if \drop
	addl	$(\drop), %esp
.endif
	popl	%edx
	popl	%ecx
	popl	%eax
.endm

.macro __afterburn_save_all
	/* The order matches that of frame_t in ia32/cpu.h. */
	/* TODO: consider using pusha and popa, to use a single instruction. */
	pushfl
	pushl	%edi
	pushl	%esi
	pushl	%ebp
	pushl	%esp
	pushl	%ebx
	pushl	%edx
	pushl	%ecx
	pushl	%eax
.endm

.macro __afterburn_restore_all drop
.if \drop
	addl	$(\drop), %esp
.endif
	popl	%eax
	popl	%ecx
	popl	%edx
	popl	%ebx
	addl	$4, %esp
	popl	%ebp
	popl	%esi
	popl	%edi
	popfl
.endm

.macro vmi_area_begin name
/* Define the start of a VMI area. */
	.section .vmi_rom, "ax"
	.org vmi_rom_offset_\name
	.globl VMI_\name
	.type VMI_\name,@function
VMI_\name:
.endm

.macro vmi_stub_begin name
/* Define the prolog of a VMI stub.  */
	vmi_area_begin \name
	burn_counter VMI_\name
	burn_counter_inc VMI_\name
.endm

.macro vmi_stub_end
/* Define the epilog of a VMI stub.  */
	.previous
.endm

.macro vmi_call_relocatable func
	9191: call	\func
	.pushsection .vmi_patchups, "aw"
	.balign 4
	.long 9191b
	.popsection
.endm

.macro vmi_jmp_relocatable target
	9191: jmp	\target
	.pushsection .vmi_patchups, "aw"
	.balign 4
	.long 9191b
	.popsection
.endm

.macro vmi_stub_simple name func
/* Invoke a front-end C function that expects a burn_clobbers_frame_t as
 * the parameter.
 */
	vmi_stub_begin \name
	__afterburn_save_clobbers
	push	%esp
	subl	$8, 0(%esp)
	vmi_call_relocatable \func
	__afterburn_restore_clobbers 4
	ret
	vmi_stub_end
.endm

.macro vmi_stub_absolute name func
/* Invoke a front-end C function that expects a burn_clobbers_frame_t as
 * the parameter.
 */
	vmi_stub_begin \name
	__afterburn_save_clobbers
	push	%esp
	subl	$8, 0(%esp)
	call	*(1f)
	__afterburn_restore_clobbers 4
	ret
	1: .long \func
	vmi_stub_end
.endm


.macro vmi_stub_nop name
/* Cancel the operation. */
vmi_stub_begin \name
	ret
vmi_stub_end
.endm

.macro vmi_stub_ident name inst
/* Execute the instruction and return. */
vmi_stub_begin \name
	\inst
	ret
vmi_stub_end
.endm

#endif	/* __IA32__FRONTEND_S__ */

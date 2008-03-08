/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/amd64/frontend.S
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__IA32__FRONTEND_S__
#define __AFTERBURN_WEDGE__INCLUDE__IA32__FRONTEND_S__

#if defined(CONFIG_BURN_COUNTERS)

.macro burn_counter_inc name
/* Increment a burn counter.  */
	incl __counter__\name
.endm

.macro burn_counter name
/* Allocate a burn counter, obeying the definitions in include/burn_counters.h.
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
  pushq	%rsp		/* Define a C parameter that points at our
        			   stack frame. */
  subq	$16, (%rsp)	/* Adjust the parameter to include our 
  		         	   upcoming call frame. */
  movq (%rsp), %rdi /* pass as first argument */
  callq	\func
  __afterburn_restore_clobbers 8
  ret
.endm

.macro __afterburn_save_clobbers
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rax
.endm

.macro __afterburn_restore_clobbers drop
.if \drop
	addq	$(\drop), %rsp
.endif
  popq %rax
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
.endm

#endif	/* __AFTERBURN_WEDGE__INCLUDE__IA32__FRONTEND_S__ */

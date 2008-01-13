/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/entry.S
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

#if 0
#include INC_ARCH(frontend.S)
#include INC_WEDGE(xen_asm.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(segment.h)

/* Note: A Xen callback already places the following info on the stack:
 *  ss, esp, eflags, cs, eip
 * It is the same as iret_user_frame_t from include/ia32/cpu.h
 */

.macro xen_pagefault_fixup
#if !defined(CONFIG_XEN_2_0)
	/* Xen 2 provides the fault address on the stack.  Xen 3 puts it
	 * in a frustrating location.  To retain compatibility with our
	 * Xen 2 code, we push the fault address on the stack.
	 */
#if defined(CONFIG_SMP)
#error Retarded!!
#endif
	pushl	xen_shared_info + 8
#endif
.endm

.macro cpu_save_all
	/* The order matches that of xen_callback_frame_t in 
	 * include/kaxen/cpu.h.
	 */
	pushl	%edi
	pushl	%esi
	pushl	%ebp
	pushl	%ebx
	pushl	%edx
	pushl	%ecx
	pushl	%eax
.endm

.macro cpu_restore_all predrop postdrop
.if \predrop
	addl	$(\predrop), %esp
.endif
	popl	%eax
	popl	%ecx
	popl	%edx
	popl	%ebx
	popl	%ebp
	popl	%esi
	popl	%edi
.if \postdrop
	addl	$(\postdrop), %esp
.endif
.endm

.macro entry name section
.ifnes "\section", ""
	.section \section, "ax"
.else
	.text
.endif
	.balign 4
	.global \name
\name:
.endm

entry afterburn_exit, ".text.pte"
	cpu_restore_all 0, 12	/* Error code, fault addr, frame id. */
	iret

entry xen_event_callback_wrapper, ".text.irq"
	subl	$8, %esp	/* Error code, fault addr. */
	pushl	$256		/* Frame ID. */
	cpu_save_all
	movl	%esp, %eax	/* A pointer to the CPU save frame. */
	call	xen_event_callback
	jmp	afterburn_exit

entry xen_event_failsafe_wrapper
	subl	$8, %esp	/* Error code, fault addr. */
	pushl	$257		/* Frame ID. */
	cpu_save_all
	movl	%esp, %eax	/* A pointer to the CPU save frame. */
	call	xen_event_failsafe
	jmp	afterburn_exit

#if defined(CONFIG_IA32_FAST_VECTOR)
entry page_fault_wrapper, ".text.pte"
	xen_pagefault_fixup

#if defined(CONFIG_KAXEN_UNLINK_HEURISTICS)
	testl	$1, (4)(%esp)		/* Error code: is it a valid pgent? */
	jnz	1f			/* Valid pgent. */
	pushl	%eax			/* Save eax. */
	movl	4(%esp), %eax		/* Get the faulting address. */
	shr	$22, %eax		/* Convert to 4MB address. */
	movl	mapping_base_region(, %eax, 4), %eax	/* Get its pdir entry. */
	andl	$((1<<0) | (1<<11)), %eax	/* Is it unlinked? */
	cmp	$((1<<11)), %eax	/* Is it unlinked? */
	jne	2f			/* Jump if not unlinked. */

	/* Call C code to perform the relink. */
	pushl	%ecx
	pushl	%edx
	mov	%esp, %eax		/* First parameter. */
	call	page_fault_relink	/* Relink. */
	__afterburn_restore_clobbers 0

	/* Return to user. */
	addl	$8, %esp		/* Remove fault addr and error code. */
	iret

2:
	popl	%eax			/* Restore eax. */
#endif

1:
	testl	$3, (12)(%esp)		/* Did we come from user? */
	jnp	page_fault_slow

	burn_counter pgfault_from_user
	burn_counter_inc pgfault_from_user
	popl	vcpu + OFS_CPU_CR2	/* Put the fault address into CR2. */
	btr	$9, vcpu + OFS_CPU_FLAGS  /* Disable interrupts (we assume). */
	jmp	*pgfault_gate_ip

page_fault_slow:
	burn_counter pgfault_from_kernel
	burn_counter_inc pgfault_from_kernel

#else

entry page_fault_wrapper, ".text.pte"
	xen_pagefault_fixup
#endif
	pushl	$(14 | (1 << 31))	/* Frame ID, error code enabled. */
	cpu_save_all
	movl	%esp, %eax		/* A pointer to the CPU save frame. */
	call	page_fault_trap
	jmp	afterburn_exit

#ifdef CONFIG_VMI_SUPPORT
entry trap_wrapper_vmi_boot
	subl	$4, %esp		/* Fault address. */
	pushl	$(13 | (1 << 31))	/* Frame ID, error code enabled. */
	cpu_save_all
	movl	%esp, %eax		/* A pointer to the CPU save frame. */
	call	vmi_trap
	jmp	afterburn_exit
#endif

#ifdef CONFIG_DEBUGGER
entry trap_wrapper_kdb
	subl	$8, %esp	/* Fault addr, error code. */
	pushl	$3		/* Frame ID. */
	cpu_save_all
	movl	%esp, %eax	/* A pointer to the CPU save frame. */
	call	debugger_enter
	jmp	afterburn_exit
#endif

.macro trap_wrapper id, use_error_code
entry trap_wrapper_\id
.if \use_error_code
	subl	$4, %esp	/* Fault addr. */
.else
	subl	$8, %esp	/* Error code, fault addr. */
.endif
	pushl	$(\id | (\use_error_code << 31))	/* Frame ID. */
	cpu_save_all
	movl	%esp, %eax	/* A pointer to the CPU save frame. */
	call	trap
	jmp	afterburn_exit
.endm

.macro wedge_syscall_wrapper id
entry wedge_syscall_wrapper_\id
	cmp	%eax, max_wedge_syscall
	jle	1f	/* Invalid system call number. */

	pushl	wedge_syscall_table(,%eax,4)	/* Load the syscall pointer. */

	/* eax has the first parameter. */
	movl	%esp, %eax	/* Build a pointer to the iret frame. */
	addl	$4, %eax	/* Locate the iret frame on the stack. */

	/* Parameter #2 is in edx.  Parameter $3 is in ecx. */
	push	%edi		/* Potential parameter, #6. */
	push	%esi		/* Potential parameter, #5 */
	push	%ebx		/* Potential parameter, #4. */

	call	*12(%esp)

	pop	%ebx		/* Restore parameter. */
	pop	%esi		/* Restore parameter. */
	pop	%edi		/* Restore parameter. */
	addl	$4, %esp	/* Remove the syscall pointer. */

	iret

1:
	mov	$2, %eax	/* Illegal system call. */
	iret
.endm

#if !defined(CONFIG_KAXEN_INT_FP)

.macro soft_trap_wrapper id
entry int_wrapper_\id
	subl	$8, %esp	/* Error code, fault addr. */
	pushl	$\id		/* Frame ID. */
	cpu_save_all
	movl	%esp, %eax	/* A pointer to the CPU save frame. */
	call	soft_trap
	jmp	afterburn_exit
.endm

#else	/* CONFIG_KAXEN_INT_FP */

#define STACK_SAVE	12
#define LRET_SIZE	8
#define OFS_IRET_IP	(0 + STACK_SAVE + LRET_SIZE)
#define OFS_IRET_CS	(4 + STACK_SAVE + LRET_SIZE)
#define OFS_IRET_FLAGS	(8 + STACK_SAVE + LRET_SIZE)
#define OFS_IRET_SP	(12 + STACK_SAVE + LRET_SIZE)
#define OFS_IRET_SS	(16 + STACK_SAVE + LRET_SIZE)
#define OFS_LRET_IP	(0 + STACK_SAVE)
#define OFS_LRET_CS	(4 + STACK_SAVE)

/* Assumption: this handler is only called for ring crossings, and thus
 * the stack includes SS and ESP.  The kernel never directly executes the
 * int instruction due to afterburning.
 */
.macro soft_trap_wrapper id
entry int_wrapper_\id
	/* Allocate space for lret frame. */
	pushl	$(XEN_CS_KERNEL)
	subl	$4, %esp

	/* Create some scratch registers. */
	pushl	%eax
	pushl	%edx
	pushl	%ecx

	/* The iret frame on the stack has physical CS and SS.  Replace
	 * with the virtual CS and SS, which are currently in the cpu object. */
#if defined(GUEST_CS_USER_SEGMENT)
	movl	$((GUEST_CS_USER_SEGMENT << 3) | 3), OFS_IRET_CS(%esp)
#else
	movl	OFS_CPU_CS + vcpu, %ecx
	movl	%ecx, OFS_IRET_CS(%esp)
#endif
#if defined(GUEST_DS_USER_SEGMENT)
	movl	$((GUEST_DS_USER_SEGMENT << 3) | 3), OFS_IRET_SS(%esp)
#else
	movl	OFS_CPU_SS + vcpu, %edx
	movl	%edx, OFS_IRET_SS(%esp)
#endif

	/* Load the tss's ss0.  Store into the virtual CPU. */
#if defined(GUEST_DS_KERNEL_SEGMENT)
	movl	$((GUEST_DS_KERNEL_SEGMENT << 3) | 0), OFS_CPU_SS + vcpu
#else
	movl	OFS_CPU_TSS + vcpu, %eax	/* Get the TSS base. */
	movzxw	8(%eax), %ecx			/* Get ss0. */
	movl	%ecx, OFS_CPU_SS + vcpu		/* Store in vcpu.ss. */
#endif

	/* Load the gate's CS.  Store into the virtual CPU.  */
	movl	OFS_CPU_IDTR + 2 + vcpu, %eax	/* eax = IDT base */
#if defined(GUEST_CS_KERNEL_SEGMENT)
	movl	$((GUEST_CS_KERNEL_SEGMENT << 3) | 0), OFS_CPU_CS + vcpu
#else
	movzxw	(\id * 8 + 2)(%eax), %ecx
	movl	%ecx, OFS_CPU_CS + vcpu	/* cpu.cs = gate.cs */
#endif

	/* Load the gate's target IP.  eax = IDT base. */
	movl	(\id * 8)(%eax), %ecx
	andl	$0xffff, %ecx
	movl	(\id * 8 + 4)(%eax), %edx
	andl	$0xffff0000, %edx
	orl	%ecx, %edx			/* edx = gate IP */
	/* Create lret frame. */
	movl	%edx, OFS_LRET_IP(%esp)

#if !defined(CONFIG_LINUX_HEURISTICS)
	/* The iret frame on the stack has physical flags.  Combine with the
	 * virtual flags stored in the cpu object. */
	andl	$0xffc08cff, OFS_IRET_FLAGS(%esp) /* Remove the sys flags. */
	movl	OFS_CPU_FLAGS + vcpu, %ecx    /* ecx = cpu.flags */
	andl	$0x00083200, %ecx	    /* Extract the system flags. */
	orl	%ecx, OFS_IRET_FLAGS(%esp)  /* Combine user and sys flags. */
#endif

	/* Restore the scratch registers. */
	popl	%ecx
	popl	%edx
	popl	%eax

	lret
.endm

#endif	/* CONFIG_KAXEN_INT_FP */


/* User defined interrupts: 32 to 255 */
#if !defined(CONFIG_IA32_FAST_VECTOR)
soft_trap_wrapper 0x80
#endif
soft_trap_wrapper 0x20
soft_trap_wrapper 0x21
wedge_syscall_wrapper 0x69

trap_wrapper id=0, use_error_code=0	/* divide by 0 */
trap_wrapper id=1, use_error_code=0	/* debug exception */
trap_wrapper id=2, use_error_code=0	/* NMI exception */
trap_wrapper id=3, use_error_code=0	/* Breakpoint exception */
trap_wrapper id=4, use_error_code=0	/* Overflow exception */
trap_wrapper id=5, use_error_code=0	/* Bound range exception */
trap_wrapper id=6, use_error_code=0	/* Invalid opcode exception */
trap_wrapper id=7, use_error_code=0	/* Device not available exception */
trap_wrapper id=8, use_error_code=1	/* Double fault exception */
trap_wrapper id=9, use_error_code=0	/* Coprocessor segment overrun */
trap_wrapper id=10, use_error_code=1	/* Invalid TSS exception */
trap_wrapper id=11, use_error_code=1	/* Segment not present */
trap_wrapper id=12, use_error_code=1	/* Stack fault exception */
trap_wrapper id=13, use_error_code=1	/* general protection fault */
trap_wrapper id=16, use_error_code=0	/* x87 FPU */
trap_wrapper id=17, use_error_code=1	/* Alignment check */
trap_wrapper id=18, use_error_code=0	/* Machine-check */
trap_wrapper id=19, use_error_code=0	/* SIMD floating-point */
#endif

/*
 * Copyright (c) 2005, VMware, Inc.
 * Copyright (c) 2005-2006, Volkmar Uhlig
 * Copyright (c) 2005-2006, University of Karlsruhe
 * All rights reserved.
 */

/**********************************************************************
 *
 *  Define the ROM layout: The offsets for all of the entries.
 *
 **********************************************************************/
	.section .vmi_rom, "ax"
	.global _VMI_ROM_START
	.balign 32
_VMI_ROM_START:

/* Include the ROM definition header file, and instantiate it.
 */
	.set which, 0
#define VDEF(name) .set vmi_rom_offset_##name## , which * 32 ; .set which , which + 1 ;

#include INC_ARCH(vmiCalls.h)

VMI_CALLS

#undef VDEF

/**********************************************************************
 *
 *  Define the ROM entries.  We define them in ascending order of
 *  their ROM locations, since the assembler can't produce code 
 *  out-of-order.
 *
 **********************************************************************/

#include INC_ARCH(frontend.S)
#include INC_WEDGE(vcpu.h)

vmi_stub_begin Init
	.global _vmi_rewrite_calls_return
	__afterburn_save_clobbers
	push	%esp		/* Build the burn_clobbers_frame_t. */
	subl	$8, 0(%esp)
	call	*(1f)		/* Invoke the VMI Init function. */
_vmi_rewrite_calls_return:
	__afterburn_restore_clobbers 4
	ret
1:	.long vmi_init_ext
vmi_stub_end


/* The CPUID handler doesn't fit within the ROM, so split it across multiple
 * sites.
 */
vmi_stub_begin CPUID
	__afterburn_save_all
	pushl	%esp		/* Push the bottom of the parameter block. */
	subl	$8, 0(%esp)	/* Adjust to point at the call frame. */
	jmp	*(1f)
	1: .long vmi_cpuid_finish
vmi_stub_end

	.text
vmi_cpuid_finish:
	call	afterburn_cpu_read_cpuid_ext
	__afterburn_restore_all 4
	ret

vmi_stub_simple WRMSR, afterburn_cpu_write_msr_ext
vmi_stub_simple RDMSR, afterburn_cpu_read_msr_ext

vmi_stub_simple SetGDT, afterburn_cpu_write_gdt32_ext
vmi_stub_simple SetLDT, afterburn_cpu_lldt_ext
vmi_stub_simple SetIDT, afterburn_cpu_write_idt32_ext
vmi_stub_simple SetTR,  afterburn_cpu_ltr_ext

vmi_stub_simple GetGDT, afterburn_cpu_unimplemented_ext
vmi_stub_simple GetLDT, afterburn_cpu_unimplemented_ext
vmi_stub_simple GetIDT, afterburn_cpu_unimplemented_ext
vmi_stub_simple GetTR,  afterburn_cpu_unimplemented_ext

vmi_stub_begin WriteGDTEntry
	mov %ecx,0x0(%eax,%edx,8)
	mov %esi,0x4(%eax,%edx,8)
	ret
vmi_stub_end
	
vmi_stub_begin WriteLDTEntry
	mov %ecx,0x0(%eax,%edx,8)
	mov %esi,0x4(%eax,%edx,8)
	ret
vmi_stub_end	

vmi_stub_begin WriteIDTEntry
	mov %ecx,0x0(%eax,%edx,8)
	mov %esi,0x4(%eax,%edx,8)
	ret
vmi_stub_end

#if 0
vmi_stub_simple UpdateKernelStack, vmi_esp0_update_ext
#else
vmi_stub_nop UpdateKernelStack
#endif

vmi_stub_simple SetCR0, afterburn_cpu_write_cr0_regs
vmi_stub_simple SetCR2, afterburn_cpu_write_cr2_regs
vmi_stub_simple SetCR3, afterburn_cpu_write_cr3_regs
vmi_stub_simple SetCR4, afterburn_cpu_write_cr4_regs

vmi_stub_simple GetCR0, afterburn_cpu_read_cr0_ext
vmi_stub_simple GetCR2, afterburn_cpu_read_cr2_ext
vmi_stub_simple GetCR3, afterburn_cpu_read_cr3_ext
vmi_stub_simple GetCR4, afterburn_cpu_read_cr4_ext

vmi_stub_simple INVD, afterburn_cpu_unimplemented_ext
vmi_stub_simple WBINVD, afterburn_cpu_unimplemented_ext

vmi_stub_simple SetDR, afterburn_cpu_write_dr_regs
vmi_stub_simple GetDR, afterburn_cpu_read_dr_regs

vmi_stub_ident RDPMC, rdpmc
vmi_stub_ident RDTSC, rdtsc

vmi_stub_simple CLTS, afterburn_cpu_clts

#if defined(CONFIG_SMP) 
#error Unimplemented
#else
vmi_stub_begin EnableInterrupts
	bts	$9, OFS_CPU_FLAGS + vcpu
	ret
vmi_stub_end
vmi_stub_begin DisableInterrupts
	btr	$9, OFS_CPU_FLAGS + vcpu
	ret
vmi_stub_end
vmi_stub_begin GetInterruptMask
	mov	OFS_CPU_FLAGS + vcpu, %eax
	ret
vmi_stub_end
vmi_stub_begin SetInterruptMask
	mov	%eax, OFS_CPU_FLAGS + vcpu
	ret
vmi_stub_end
#endif

/* JTL: VMI uses a call to the iret; we need to rewrite to use a jmp. */
vmi_stub_begin IRET
#if defined(CONFIG_IA32_FAST_VECTOR)
	/* We have come here via a call, remove return address */
	add $4, %esp
	vmi_jmp_relocatable	burn_iret
#else
	/* We have come here via a call, remove address and add 3 params */
	subl	$(12-4), %esp	/* Allocate space for redirect frame. */
	__afterburn_save_all
	movl	%esp, %eax	/* 1st parameter: iret_handler_frame_t */
	vmi_jmp_relocatable	afterburn_cpu_iret
#endif
vmi_stub_end

vmi_stub_simple SYSEXIT, afterburn_cpu_unimplemented_ext
vmi_stub_ident Pause, pause

vmi_stub_begin Halt
#if defined(CONFIG_SMP)
#error Unimplemented
#else
	/* JTL: does VMI still do a SafeHalt? */
	bts	$9, OFS_CPU_FLAGS + vcpu
#endif
	vmi_jmp_relocatable burn_interruptible_hlt
vmi_stub_end

vmi_stub_simple Reboot, vmi_cpu_reboot
vmi_stub_simple Shutdown, vmi_cpu_shutdown
#if 1
vmi_stub_simple SetPxE, vmi_set_pte_ext
#else
vmi_stub_begin SetPxE
	push	%edx
	push	%ecx
	push	%eax
	vmi_call_relocatable backend_vmi_write_patch
	pop	%eax
	pop	%ecx
	pop	%edx
	ret
vmi_stub_end
#endif
#if 1
vmi_stub_begin GetPxE
	testl	%eax, %eax
	jz	1f
	/* Assume: 
	 *   extern "C" word_t __attribute__((regparm(2)))
	 *   backend_pte_normalize_patch( pgent_t pgent, int level )
	 */
	push	%edx
	push	%ecx
	vmi_call_relocatable backend_pte_normalize_patch
	pop	%ecx
	pop	%edx
1:	
	ret
vmi_stub_end
#else
vmi_stub_simple GetPxE, vmi_get_pxe_ext
#endif


vmi_stub_begin SwapPxE
	/* Assume: 
	 *   extern "C" word_t __attribute__((regparm(2)))
	 *   backend_pte_xchg_patch( word_t new_val, pgent_t *pgent )
	 */
	push	%edx
	push	%ecx
	xchg	%eax, %edx /* swap params */
	vmi_call_relocatable backend_pte_xchg_patch
	pop	%ecx
	pop	%edx
	ret
vmi_stub_end

vmi_stub_simple SetPxELong, afterburn_cpu_unimplemented_ext
vmi_stub_simple GetPxELong, afterburn_cpu_unimplemented_ext
vmi_stub_simple SwapPxELongAtomic, afterburn_cpu_unimplemented_ext

vmi_stub_simple TestAndSetPxEBit, vmi_test_and_set_pte_bit_ext

vmi_stub_begin TestAndClearPxEBit
	/* Assume: 
	 *   extern "C" word_t __attribute__((regparm(2)))
	 *   backend_pte_test_clear_patch( word_t bit, pgent_t *pgent )
	 */
	push	%edx
	push	%ecx
	xchg	%eax, %edx /* swap params */
	vmi_call_relocatable backend_pte_test_clear_patch
	pop	%ecx
	pop	%edx
	ret
vmi_stub_end

vmi_stub_nop AllocatePage
vmi_stub_nop ReleasePage

vmi_stub_simple InvalPage, afterburn_cpu_invlpg_ext
vmi_stub_simple FlushTLB, vmi_flush_tlb_ext
vmi_stub_nop FlushDeferredCalls
vmi_stub_begin SetLinearMapping
	ret
vmi_stub_end
//vmi_stub_nop SetLinearMapping

.macro vmi_stub_outs type bitwidth
	vmi_stub_begin OUTS\type
	push	%esi
	__afterburn_save_clobbers
	push	%esp
	subl	$8, 0(%esp)
	vmi_call_relocatable vmi_cpu_write_port_string_\bitwidth
	__afterburn_restore_clobbers 4
	pop	%esi
	ret
	vmi_stub_end
.endm

.macro vmi_stub_ins type bitwidth
	vmi_stub_begin INS\type
	push	%edi
	__afterburn_save_clobbers
	push	%esp
	subl	$8, 0(%esp)
	vmi_call_relocatable vmi_cpu_read_port_string_\bitwidth
	__afterburn_restore_clobbers 4
	pop	%edi
	ret
	vmi_stub_end
.endm

vmi_stub_simple IN, afterburn_cpu_read_port32_regs
vmi_stub_simple INB, afterburn_cpu_read_port8_regs
vmi_stub_simple INW, afterburn_cpu_read_port16_regs
vmi_stub_ins , 32
vmi_stub_ins B, 8
vmi_stub_ins W, 16
vmi_stub_simple OUT, afterburn_cpu_write_port32_regs
vmi_stub_simple OUTB, afterburn_cpu_write_port8_regs
vmi_stub_simple OUTW, afterburn_cpu_write_port16_regs
vmi_stub_outs , 32
vmi_stub_outs B, 8
vmi_stub_outs W, 16


vmi_stub_begin SetIOPLMask
	pushf
	andb $0xcf, 1(%esp)
	orb  %ah, 1(%esp)	/* User error could turn on NT - not really an issue */
	popf
	ret
vmi_stub_end

vmi_stub_simple DeactivatePxELongAtomic, afterburn_cpu_unimplemented_ext
vmi_stub_simple TestAndSetPxELongBit, afterburn_cpu_unimplemented_ext
vmi_stub_simple TestAndClearPxELongBit, afterburn_cpu_unimplemented_ext

vmi_stub_simple SetInitialAPState, afterburn_cpu_unimplemented_ext

#ifdef CONFIG_DEVICE_APIC
vmi_stub_begin APICWrite
	/* extern "C" void __attribute__((regparm(2))) */
	push	%edx
	push	%ecx
	push	%eax
	xchg	%eax, %edx	/* different calling convention */
	vmi_call_relocatable lapic_write_patch
	pop	%eax
	pop	%ecx
	pop	%edx
	ret
vmi_stub_end

vmi_stub_begin APICRead
	/* extern "C" word_t __attribute__((regparm(1))) */
	push	%edx
	push	%ecx
	vmi_call_relocatable lapic_read_patch
	pop	%ecx
	pop	%edx
	ret
vmi_stub_end	

#else
vmi_stub_nop APICWrite
vmi_stub_begin APICRead
	mov $-1, %eax /* no APIC connected */
	ret
vmi_stub_end
#endif

vmi_stub_begin IODelay
	outb	%al, $0x80
	ret
vmi_stub_end

vmi_stub_simple GetCycleFrequency, vmi_get_cpu_hz
vmi_stub_ident GetCycleCounter, rdtsc
vmi_stub_simple SetAlarm, afterburn_cpu_unimplemented_ext
vmi_stub_simple CancelAlarm, afterburn_cpu_unimplemented_ext
vmi_stub_simple GetWallclockTime, afterburn_cpu_unimplemented_ext
vmi_stub_simple WallclockUpdated, afterburn_cpu_unimplemented_ext

vmi_stub_simple PhysToMachine, vmi_phys_to_machine_ext
vmi_stub_simple MachineToPhys, vmi_machine_to_phys_ext
vmi_stub_simple PhysIsCoherent, vmi_phys_is_coherent_ext

	
	.section .vmi_rom, "ax"
	.global _VMI_ROM_END
	.balign 32
_VMI_ROM_END:


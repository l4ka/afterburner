/*********************************************************************
 *
 * Copyright (C) 2006,  Karlsruhe University
 *
 * File path:     testvirt/ia32_instructions.h
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
 * $Id:$
 *
 ********************************************************************/
#ifndef __IA32__IA32_H__
#define __IA32__IA32_H__

#include <macros.h>
#include INC_ARCH(types.h)

/**********************************************************************
 *    control register bits (CR0, CR3, CR4)
 **********************************************************************/

#define X86_CR0_PE 	(__UL(1) <<  0)   /* enable protected mode            */
#define X86_CR0_MP 	(__UL(1) <<  1)   /* monitor coprocessor              */
#define X86_CR0_EM 	(__UL(1) <<  2)   /* disable fpu                      */
#define X86_CR0_TS 	(__UL(1) <<  3)   /* task switched                    */
#define X86_CR0_ET 	(__UL(1) <<  4)   /* extension type (always 1)        */
#define X86_CR0_NE 	(__UL(1) <<  5)   /* numeric error reporting mode     */
#define X86_CR0_WP 	(__UL(1) << 16)   /* force write protection on user
					     read only pages for kernel       */
#define X86_CR0_AM	(__UL(1) << 3)	  /* alignment mask		      */
#define X86_CR0_NW 	(__UL(1) << 29)   /* not write through                */
#define X86_CR0_CD 	(__UL(1) << 30)   /* cache disabled                   */
#define X86_CR0_PG 	(__UL(1) << 31)   /* enable paging                    */

#define X86_CR3_PCD    	(__UL(1) <<  3)   /* page-level cache disable     */
#define X86_CR3_PWT    	(__UL(1) <<  4)   /* page-level writes transparent*/

#define X86_CR4_VME    	(__UL(1) <<  0)   /* virtual 8086 mode extension  */
#define X86_CR4_PVI    	(__UL(1) <<  1)   /* enable protected mode
					     virtual interrupts           */
#define X86_CR4_TSD    	(__UL(1) <<  2)   /* time stamp disable           */
#define X86_CR4_DE     	(__UL(1) <<  3)   /* debug extensions             */
#define X86_CR4_PSE    	(__UL(1) <<  4)   /* page size extension (4MB)    */
#define X86_CR4_PAE    	(__UL(1) <<  5)   /* physical address extension   */
#define X86_CR4_MCE    	(__UL(1) <<  6)   /* machine check extensions     */
#define X86_CR4_PGE    	(__UL(1) <<  7)   /* enable global pages          */
#define X86_CR4_PCE    	(__UL(1) <<  8)   /* allow user to use rdpmc      */
#define X86_CR4_OSFXSR 	(__UL(1) <<  9)   /* enable fxsave/fxrstor + sse  */
#define X86_CR4_OSXMMEXCPT (__UL(1) << 10) /* support for unmsk. SIMD exc. */
#define X86_CR4_VMXE   	(__UL(1) << 13)	  /* vmx extensions                */

/**********************************************************************
 *    FLAGS register
 **********************************************************************/
#define X86_FLAGS_CF    (__UL(1) <<  0)       /* carry flag                   */
#define X86_FLAGS_PF    (__UL(1) <<  2)       /* parity flag                  */
#define X86_FLAGS_AF    (__UL(1) <<  4)       /* auxiliary carry flag         */
#define X86_FLAGS_ZF    (__UL(1) <<  6)       /* zero flag                    */
#define X86_FLAGS_SF    (__UL(1) <<  7)       /* sign flag                    */
#define X86_FLAGS_TF    (__UL(1) <<  8)       /* trap flag                    */
#define X86_FLAGS_IF    (__UL(1) <<  9)       /* interrupt enable flag        */
#define X86_FLAGS_DF    (__UL(1) << 10)       /* direction flag               */
#define X86_FLAGS_OF    (__UL(1) << 11)       /* overflow flag                */
#define X86_FLAGS_NT    (__UL(1) << 14)       /* nested task flag             */
#define X86_FLAGS_RF    (__UL(1) << 16)       /* resume flag                  */
#define X86_FLAGS_VM    (__UL(1) << 17)       /* virtual 8086 mode            */
#define X86_FLAGS_AC    (__UL(1) << 18)       /* alignement check             */
#define X86_FLAGS_VIF   (__UL(1) << 19)       /* virtual interrupt flag       */
#define X86_FLAGS_VIP   (__UL(1) << 20)       /* virtual interrupt pending    */
#define X86_FLAGS_ID    (__UL(1) << 21)       /* CPUID flag                   */
#define X86_FLAGS_IOPL(x)       ((x & 3) << 12) /* the IO privilege level field */

/* Intel-defined CPU features, CPUID func 1 word ECX */
#define X86_FEATURE_XMM3	(__UL(1) << 0) /* Streaming SIMD Extensions-3 */
#define X86_FEATURE_MWAIT	(__UL(1) << 3) /* Monitor/Mwait support */
#define X86_FEATURE_DSCPL	(__UL(1) << 4) /* CPL Qualified Debug Store */
#define X86_FEATURE_VMXE	(__UL(1) << 5) /* Virtual Machine Extensions */
#define X86_FEATURE_SMXE	(__UL(1) << 6) /* Safer Mode Extensions */
#define X86_FEATURE_EST		(__UL(1) << 7) /* Enhanced SpeedStep */
#define X86_FEATURE_TM2		(__UL(1) << 8) /* Thermal Monitor 2 */
#define X86_FEATURE_SSSE3	(__UL(1) << 9) /* Supplemental Streaming SIMD Extensions-3 */
#define X86_FEATURE_CID		(__UL(1) <<10) /* Context ID */
#define X86_FEATURE_CX16        (__UL(1) <<13) /* CMPXCHG16B */
#define X86_FEATURE_XTPR	(__UL(1) <<14) /* Send Task Priority Messages */
#define X86_FEATURE_PDCM	(__UL(1) <<15) /* Perf/Debug Capability MSR */
#define X86_FEATURE_DCA		(__UL(1) <<18) /* Direct Cache Access */
#define X86_FEATURE_SSE4_1	(__UL(1) <<19) /* Streaming SIMD Extensions 4.1 */
#define X86_FEATURE_SSE4_2	(__UL(1) <<20) /* Streaming SIMD Extensions 4.2 */
#define X86_FEATURE_POPCNT	(__UL(1) <<23) /* POPCNT instruction */

/* Intel-defined CPU features, CPUID func 1 word EDX */
#define X86_FEATURE_FPU		(__UL(1) << 0) /* Onboard FPU */
#define X86_FEATURE_VME		(__UL(1) << 1) /* Virtual Mode Extensions */
#define X86_FEATURE_DE		(__UL(1) << 2) /* Debugging Extensions */
#define X86_FEATURE_PSE 	(__UL(1) << 3) /* Page Size Extensions */
#define X86_FEATURE_TSC		(__UL(1) << 4) /* Time Stamp Counter */
#define X86_FEATURE_MSR		(__UL(1) << 5) /* Model-Specific Registers, RDMSR, WRMSR */
#define X86_FEATURE_PAE		(__UL(1) << 6) /* Physical Address Extensions */
#define X86_FEATURE_MCE		(__UL(1) << 7) /* Machine Check Architecture */
#define X86_FEATURE_CX8		(__UL(1) << 8) /* CMPXCHG8 instruction */
#define X86_FEATURE_APIC	(__UL(1) << 9) /* Onboard APIC */
#define X86_FEATURE_SEP		(__UL(1) <<11) /* SYSENTER/SYSEXIT */
#define X86_FEATURE_MTRR	(__UL(1) <<12) /* Memory Type Range Registers */
#define X86_FEATURE_PGE		(__UL(1) <<13) /* Page Global Enable */
#define X86_FEATURE_MCA		(__UL(1) <<14) /* Machine Check Architecture */
#define X86_FEATURE_CMOV	(__UL(1) <<15) /* CMOV instruction (FCMOVCC and FCOMI too if FPU present) */
#define X86_FEATURE_PAT		(__UL(1) <<16) /* Page Attribute Table */
#define X86_FEATURE_PSE36	(__UL(1) <<17) /* 36-bit PSEs */
#define X86_FEATURE_PN		(__UL(1) <<18) /* Processor serial number */
#define X86_FEATURE_CLFLSH	(__UL(1) <<19) /* Supports the CLFLUSH instruction */
#define X86_FEATURE_DS		(__UL(1) <<21) /* Debug Store */
#define X86_FEATURE_ACPI	(__UL(1) <<22) /* ACPI via MSR */
#define X86_FEATURE_MMX		(__UL(1) <<23) /* Multimedia Extensions */
#define X86_FEATURE_FXSR	(__UL(1) <<24) /* FXSAVE and FXRSTOR instructions (fast save and restore */
				          /* of FPU context), and CR4.OSFXSR available */
#define X86_FEATURE_XMM		(__UL(1) <<25) /* Streaming SIMD Extensions */
#define X86_FEATURE_XMM2	(__UL(1) <<26) /* Streaming SIMD Extensions-2 */
#define X86_FEATURE_SELFSNOOP	(__UL(1) <<27) /* CPU self snoop */
#define X86_FEATURE_HT		(__UL(1) <<28) /* Hyper-Threading */
#define X86_FEATURE_ACC		(__UL(1) <<29) /* Automatic clock control */
#define X86_FEATURE_IA64	(__UL(1) <<30) /* IA-64 processor */


/* AMD-defined CPU features: CPUID func 0x80000001 word ECX */
#define X86_FEATURE_LAHF_LM	(__UL(1) << 0) /* LAHF/SAHF in long mode */
#define X86_FEATURE_CMP_LEGACY	(__UL(1) << 1) /* If yes HyperThreading not valid */
#define X86_FEATURE_SVME        (__UL(1) << 2) /* Secure Virtual Machine */
#define X86_FEATURE_EXTAPICSPACE (__UL(1) << 3) /* Extended APIC space */
#define X86_FEATURE_ALTMOVCR	(__UL(1) << 4) /* LOCK MOV CR accesses CR+8 */
#define X86_FEATURE_ABM		(__UL(1) << 5) /* Advanced Bit Manipulation */
#define X86_FEATURE_SSE4A	(__UL(1) << 6) /* AMD Streaming SIMD Extensions-4a */
#define X86_FEATURE_MISALIGNSSE	(__UL(1) << 7) /* Misaligned SSE Access */
#define X86_FEATURE_3DNOWPF	(__UL(1) << 8) /* 3DNow! Prefetch */
#define X86_FEATURE_OSVW	(__UL(1) << 9) /* OS Visible Workaround */
#define X86_FEATURE_SKINIT	(__UL(1) << 12) /* SKINIT, STGI/CLGI, DEV */
#define X86_FEATURE_WDT		(__UL(1) << 13) /* Watchdog Timer */


/* AMD-defined CPU features, CPUID func 0x80000001, word EDX */
#define X86_FEATURE_SYSCALL	(__UL(1) <<11) /* SYSCALL/SYSRET */
#define X86_FEATURE_MP		(__UL(1) <<19) /* MP Capable. */
#define X86_FEATURE_NX		(__UL(1) <<20) /* Execute Disable */
#define X86_FEATURE_MMXEXT	(__UL(1) <<22) /* AMD MMX extensions */
#define X86_FEATURE_FFXSR       (__UL(1) <<25) /* FFXSR instruction optimizations */
#define X86_FEATURE_PAGE1GB	(__UL(1) <<26) /* 1Gb large page support */
#define X86_FEATURE_RDTSCP	(__UL(1) <<27) /* RDTSCP */
#define X86_FEATURE_LM		(__UL(1) <<29) /* Long Mode (x86-64) */
#define X86_FEATURE_3DNOWEXT	(__UL(1) <<30) /* AMD 3DNow! extensions */
#define X86_FEATURE_3DNOW	(__UL(1) <<31) /* 3DNow! */


/**********************************************************************
 *    EXCEPTION error codes
 **********************************************************************/
#define X86_EXC_DIVIDE_ERROR        0
#define X86_EXC_DEBUG               1
#define X86_EXC_NMI                 2
#define X86_EXC_BREAKPOINT          3
#define X86_EXC_OVERFLOW            4
#define X86_EXC_BOUNDRANGE          5
#define X86_EXC_INVALIDOPCODE       6
#define X86_EXC_NOMATH_COPROC       7
#define X86_EXC_DOUBLEFAULT         8
#define X86_EXC_COPSEG_OVERRUN      9
#define X86_EXC_INVALID_TSS         10
#define X86_EXC_SEGMENT_NOT_PRESENT 11
#define X86_EXC_STACKSEG_FAULT      12
#define X86_EXC_GENERAL_PROTECTION  13
#define X86_EXC_PAGEFAULT           14
#define X86_EXC_RESERVED            15
#define X86_EXC_FPU_FAULT           16
#define X86_EXC_ALIGNEMENT_CHECK    17
#define X86_EXC_MACHINE_CHECK       18
#define X86_EXC_SIMD_FAULT          19

/* Intel reserved exceptions */
#define X86_EXC_RESERVED_FIRST		20
#define X86_EXC_RESERVED_LAST		31


/**********************************************************************
 *    MSRS
 **********************************************************************/
#define X86_MSR_APIC_BASE			0x0000001b
#define X86_MSR_UCODE_REV         		0x0000008b
#define X86_MSR_SYSENTER_CS         		0x00000174
#define X86_MSR_SYSENTER_EIP        		0x00000176
#define X86_MSR_SYSENTER_ESP        		0x00000175



INLINE void ia32_cpuid(word_t index,
		       word_t* eax, word_t* ebx, word_t* ecx, word_t* edx)
{
    __asm__ (
	"cpuid"
	: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
	: "a" (index)
	);
}


INLINE u64_t ia32_rdtsc (void)
{
    u64_t value;

    __asm__ __volatile__ ("rdtsc" : "=A"(value));

    return value;
}


#endif /* !__IA32__IA32_H__ */

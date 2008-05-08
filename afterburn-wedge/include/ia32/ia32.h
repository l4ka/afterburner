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

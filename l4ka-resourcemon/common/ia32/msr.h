
///////////////////////////////////////////////////////////////////////////
//
// msr.h, last modified 27.06.05, Marcus Reinhardt
//
///////////////////////////////////////////////////////////////////////////
//
// Contains some definitions required for the performance counters
// derived from abyss
//
// Author:  Brinkley Sprunt
//          bsprunt@bucknell.edu
//          http://www.eg.bucknell.edu/~bsprunt/
//
//////////////////////////////////////////////////////////////////////////
#ifndef __IA32__MSR_H__
#define __IA32__MSR_H__

#define CMD_PC_READ_MSR		 		 5
#define CMD_PC_WRITE_MSR			10
#define CMD_PC_READ_TSC				15
#define CMD_PC_READ_PMC				20
#define CMD_PC_READ_CR				25

#endif	/* DEF_PERF_COUNTERS_H */

#ifndef ASM_PERF_COUNTERS_H
#define ASM_PERF_COUNTERS_H

#include <l4/types.h>
#include <common/ia32/msr_def.h>

/* Note that none of these macros has a trailing semicolon (";") */

/*
 * Macros for enabling and disabling usr access to RDTSC and RDPMC.
 */
#define	PC_ENABLE_USR_RDPMC		__asm__ __volatile__ (		/* enabled when PCE bit set */ \
					"movl %%cr4,%%eax\n\t"\
					"orl $0x00000100,%%eax\n\t"\
					"movl %%eax,%%cr4"\
					: : : "eax")

#define	PC_DISABLE_USR_RDPMC		__asm__ __volatile__ (		/* disabled when PCE bit clear */ \
					"movl %%cr4,%%eax\n\t"\
					"andl $0xfffffeff,%%eax\n\t"\
					"movl %%eax,%%cr4"\
					: : : "eax")

#define	PC_ENABLE_USR_RDTSC		__asm__ __volatile__ (		/* enabled when TSD bit clear */ \
					"movl %%cr4,%%eax\n\t"\
					"andl $0xfffffffb,%%eax\n\t"\
					"movl %%eax,%%cr4"\
					: : : "eax")

#define	PC_DISABLE_USR_RDTSC		__asm__ __volatile__ (		/* disabled when TSD bit set */ \
					"movl %%cr4,%%eax\n\t"\
					"orl $0x000000004,%%eax\n\t"\
					"movl %%eax,%%cr4"\
					: : : "eax")

/////////////////////////////////////////////////////////////////////////////////
// The general structure of the GNU asm() is
//   "code"
//   : output registers (with destination)
//   : input registers (with source)
//   : other affected registers
//
// For more information see 'info gcc', especially the Extended ASM discussion.
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
//
// generic instruction sequence:
//    - buf must be abyss_uint64_t
//    - N must be valid for the first instruction
//
/////////////////////////////////////////////////////////////////////////////////
#define PC_ASM(instructions,N,buf) \
  __asm__ __volatile__ ( instructions : "=A" (buf) : "c" (N) )

/////////////////////////////////////////////////////////////////////////////////
//
// Read a model-specific register:
//    - buf must be abyss_uint64_t
//    - N must be a valid MSR number from msr_arch.h
//    - rdmsr can only be used in kernel mode
//
/////////////////////////////////////////////////////////////////////////////////
#define PC_ASM_READ_MSR(N,buf) PC_ASM("rdmsr",N,buf)

/////////////////////////////////////////////////////////////////////////////////
//
// Write a model-specific register:
//     - buf must be abyss_uint64_t
//     - N must be a valid MSR number from msr_arch.h
//     - wrmsr can only be used in kernel mode
//
/////////////////////////////////////////////////////////////////////////////////
#define PC_ASM_WRITE_MSR(N,buf) \
  __asm__ __volatile__ ( "wrmsr" : : "A" (buf), "c" (N) )

/////////////////////////////////////////////////////////////////////////////////
//
// Read the time-stamp counter:
//     - buf must be abyss_uint64_t
//     - rdtsc can be enabled for usr mode via the TSD bit in CR4
//
/////////////////////////////////////////////////////////////////////////////////
#define PC_ASM_READ_TSC(buf) \
  __asm__ __volatile__ ( "rdtsc" : "=A" (buf) )

/////////////////////////////////////////////////////////////////////////////////
// Read a performance-monitoring counter:
//    - buf must be pmc_uint64_t
//    - N must be in the range [0,pmc_event_counters)
//    - rdpmc can be enabled for usr mode via the PCE bit in CR4
//
/////////////////////////////////////////////////////////////////////////////////
#define PC_ASM_READ_PMC(N,buf) PC_ASM("rdpmc",N,buf)

/////////////////////////////////////////////////////////////////////////////////
// Read a control register:
//     - buf must be abyss_uint32_t
//     - N must be a valid control register number (0, 2, 3, 4)
//     - this can only be used in kernel mode
//
/////////////////////////////////////////////////////////////////////////////////
#define PC_ASM_READ_CR(N,buf) \
  __asm__ __volatile__ ( "movl %%cr" #N ",%0" : "=r" (buf) )


#endif /* !__IA32__MSR_H__ */

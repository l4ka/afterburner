/*
 * i386 virtual CPU header
 * 
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef CPU_I386_H
#define CPU_I386_H

#include "config.h"

#ifdef TARGET_X86_64
#define TARGET_LONG_BITS 64
#else
/* #define TARGET_LONG_BITS 32 */
#define TARGET_LONG_BITS 64 /* for Qemu map cache */
#endif

/* target supports implicit self modifying code */
#define TARGET_HAS_SMC
/* support for self modifying code even if the modified instruction is
   close to the modifying instruction */
#define TARGET_HAS_PRECISE_SMC

#include "cpu-defs.h"

#include "softfloat.h"

#if defined(__i386__) && !defined(CONFIG_SOFTMMU)
#define USE_CODE_COPY
#endif

#if defined(__i386__)

#define MSR_IA32_APICBASE               0x1b
#define MSR_IA32_APICBASE_BSP           (1<<8)
#define MSR_IA32_APICBASE_ENABLE        (1<<11)
#define MSR_IA32_APICBASE_BASE          (0xfffff<<12)
#define CPUID_APIC (1 << 9)

#endif

#ifdef USE_X86LDOUBLE
typedef floatx80 CPU86_LDouble;
#else
typedef float64 CPU86_LDouble;
#endif

typedef struct SegmentCache {
    uint32_t selector;
    target_ulong base;
    uint32_t limit;
    uint32_t flags;
} SegmentCache;

/* Empty for now */
typedef struct CPUX86State {
    uint32_t a20_mask;

    int interrupt_request;

    SegmentCache idt; /* only base and limit are used */

    /* in order to simplify APIC support, we leave this pointer to the
       user */
    struct APICState *apic_state;

    CPU_COMMON

    uint32_t cpuid_features;
} CPUX86State;

CPUX86State *cpu_x86_init(void);
int cpu_x86_exec(CPUX86State *s);
void cpu_x86_close(CPUX86State *s);
int cpu_get_pic_interrupt(CPUX86State *s);
/* MSDOS compatibility mode FPU exception support */
void cpu_set_ferr(CPUX86State *s);

void cpu_x86_set_a20(CPUX86State *env, int a20_state);

#ifndef IN_OP_I386
void cpu_x86_outb(CPUX86State *env, int addr, int val);
void cpu_x86_outw(CPUX86State *env, int addr, int val);
void cpu_x86_outl(CPUX86State *env, int addr, int val);
int cpu_x86_inb(CPUX86State *env, int addr);
int cpu_x86_inw(CPUX86State *env, int addr);
int cpu_x86_inl(CPUX86State *env, int addr);
#endif

/* helper2.c */
int main_loop(void);

#if defined(__i386__) || defined(__x86_64__)
#define TARGET_PAGE_BITS 12
#elif defined(__ia64__)
#define TARGET_PAGE_BITS 14
#endif 

/*
 *  DEFINITIONS FOR CPU BARRIERS
 */
#if defined(__i386__)
#define mb()  __asm__ __volatile__ ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define rmb() __asm__ __volatile__ ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define wmb() __asm__ __volatile__ ( "" : : : "memory")
#elif defined(__x86_64__)
#define mb()  __asm__ __volatile__ ( "mfence" : : : "memory")
#define rmb() __asm__ __volatile__ ( "lfence" : : : "memory")
#define wmb() __asm__ __volatile__ ( "" : : : "memory")
#elif defined(__ia64__)
#define mb()   __asm__ __volatile__ ("mf" ::: "memory")
#define rmb()  __asm__ __volatile__ ("mf" ::: "memory")
#define wmb()  __asm__ __volatile__ ("mf" ::: "memory")
#elif defined(__powerpc__)
/* XXX loosen these up later */
#define mb()   __asm__ __volatile__ ("sync" : : : "memory")
#define rmb()  __asm__ __volatile__ ("sync" : : : "memory") /* lwsync? */
#define wmb()  __asm__ __volatile__ ("sync" : : : "memory") /* eieio? */
#else
#error "Define barriers"
#endif


#include "cpu-all.h"

#endif /* CPU_I386_H */

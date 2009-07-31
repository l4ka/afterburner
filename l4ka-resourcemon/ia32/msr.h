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


#include <l4/types.h>
#include <ia32/msr_def.h>

INLINE L4_Word64_t x86_rdpmc(const int ctrsel)
{
    L4_Word32_t eax, edx;

    __asm__ __volatile__ (
            "rdpmc"
            : "=a"(eax), "=d"(edx)
            : "c"(ctrsel));

    return (((L4_Word64_t)edx) << 32) | (L4_Word64_t)eax;
}


INLINE L4_Word64_t x86_rdtsc(void)
{
    L4_Word32_t eax, edx;

    __asm__ __volatile__ (
            "rdtsc"
            : "=a"(eax), "=d"(edx));

    return (((L4_Word64_t)edx) << 32) | (L4_Word64_t)eax;
}


INLINE L4_Word64_t x86_rdmsr(const L4_Word32_t reg)
{
    L4_Word32_t eax, edx;

    __asm__ __volatile__ (
            "rdmsr"
            : "=a"(eax), "=d"(edx)
            : "c"(reg)
    );

    return (((L4_Word64_t)edx) << 32) | (L4_Word64_t)eax;
}


INLINE void x86_wrmsr(const L4_Word32_t reg, const L4_Word64_t val)
{
    __asm__ __volatile__ (
            "wrmsr"
            :
            : "a"( (L4_Word32_t) val), "d" ( (L4_Word32_t) (val >> 32)), "c" (reg));
}

#endif /* !__IA32__MSR_H__ */

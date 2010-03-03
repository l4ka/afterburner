/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     ia32/msr.h
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
 * $Id$
 *                
 ********************************************************************/
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

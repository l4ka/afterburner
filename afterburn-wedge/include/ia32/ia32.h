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

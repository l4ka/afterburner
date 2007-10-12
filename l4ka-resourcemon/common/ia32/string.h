/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     string.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __COMMON__IA32__STRING_H__
#define __COMMON__IA32__STRING_H__

#define HAVE_ARCH_MEMCPY
#define HAVE_ARCH_MEMSET

#include <l4/kdebug.h>
#include <common/console.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy( void *dest, const void *src, L4_Word_t n )
{
    word_t dummy1, dummy2, dummy3;
    asm volatile (
            "jecxz 1f\n"
            "repnz movsl (%%esi), (%%edi)\n"
            "1: test $3, %%edx\n"
            "jz 1f\n"
            "mov %%edx, %%ecx\n"
            "repnz movsb (%%esi), (%%edi)\n"
            "1:\n"
            : "=S"(dummy1), "=D"(dummy2), "=c"(dummy3)
            : "S"(src), "D"(dest), "c"(n >> 2), "d"(n & 3));

    return dest;
}

void *memset( void *s, L4_Word8_t c, L4_Word_t n )
{
    word_t dummy1, dummy2, dummy3;
    u8_t cccc[4] = { c, c, c, c };

    asm volatile (
            "jecxz 3f			\n"
	    "1:				\n"
            "movl %%eax, (%%edi)	\n"
	    "add $4, %%edi		\n"
	    "loop 1b			\n"
            "1: test $3, %%edx		\n"
            "jz 3f			\n"
            "mov %%edx, %%ecx		\n"
	    "2:				\n"
            "movb %%al, (%%edi)		\n"
	    "add $1, %%edi		\n"
	    "loop 2b			\n"
            "3:\n"
            : "=A"(dummy1), "=D"(dummy2), "=c"(dummy3)
            : "A"(*cccc), "D"(s), "c"(n >> 2), "d"(n & 3));

    return s;
}

#ifdef __cplusplus
}
#endif

#endif /* !__COMMON__IA32__STRING_H__ */

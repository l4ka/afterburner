/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:     afterburn-wedge/common/memory.cc
 * Description:   Basic memory block manipulations, such as copy and clear.
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

#include <console.h>
#include INC_ARCH(types.h)
#include <memory.h>
#include <burn_counters.h>

DECLARE_BURN_COUNTER(memcpy_unaligned);
DECLARE_BURN_COUNTER(memcpy_alignment_possible);

WEAK void *memset( void *mem, int c, unsigned long size )
{
    for( unsigned long idx = 0; idx < size; idx++ )
	((unsigned char *)mem)[idx] = c;
    return mem;
}

WEAK void memzero( void *mem_start, unsigned long size )
    /* mem_start must be aligned to the word size, and a multiple in length
     * of the word size.
     */
{
    for( unsigned long idx = 0; idx < size/sizeof(unsigned long); idx++ )
	((unsigned long *)mem_start)[idx] = 0;
}

WEAK void memcpy( void *dest, const void *src, unsigned long n )
{
#if defined(CONFIG_ARCH_IA32)
    word_t dummy1, dummy2, dummy3;
    if( !((word_t)dest % sizeof(word_t)) && !((word_t)src % sizeof(word_t)) ) {
	__asm__ __volatile__ (
		"jecxz 1f\n"
		"repnz movsl (%%esi), (%%edi)\n"
		"1: test $3, %%edx\n"
		"jz 2f\n"
		"mov %%edx, %%ecx\n"
		"repnz movsb (%%esi), (%%edi)\n"
		"2:\n"
		: "=S"(dummy1), "=D"(dummy2), "=c"(dummy3)
		: "S"(src), "D"(dest), "c"(n >> 2), "d"(n & 3));
	return;
    }

    INC_BURN_COUNTER( memcpy_unaligned );
    if( ((word_t)dest % sizeof(word_t)) == ((word_t)src % sizeof(word_t)) )
	INC_BURN_COUNTER( memcpy_alignment_possible );

    __asm__ __volatile__ (
	    "jecxz 1f\n"
	    "repnz movsb (%%esi), (%%edi)\n"
	    "1:\n"
	    : "=S"(dummy1), "=D"(dummy2), "=c"(dummy3)
	    : "S"(src), "D"(dest), "c"(n)
	);
    return;
#endif
    for( unsigned long i = 0; i < n; i++ )
	((unsigned char *)dest)[i] = ((unsigned char *)src)[i];
}

WEAK unsigned strlen( const char *str )
{
    unsigned len = 0;
    while( str[len] )
	len++;
    return len;
}

WEAK int strncmp(const char *s1, const char *s2, int n)
{
    if (n == 0)
	return (0);
    do {
	if (*s1 != *s2++)
	    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
	if (*s1++ == 0)
	    break;
    } while (--n != 0);
    return 0;
}

WEAK int strcmp( const char *str1, const char *str2 )
{
    while( *str1 && *str2 )
    {
	if( *str1 < *str2 )
	    return -1;
	if( *str1 > *str2 )
	    return 1;
	str1++;
	str2++;
    }
    if( *str2 )
	return -1;
    if( *str1 )
	return 1;
    return 0;
}

/*
 * Find the first occurrence of find in s.
 */
WEAK char * strstr(const char *s, const char *find)
{
    char c, sc;
    int len;

    if ((c = *find++) != 0) {
	len = strlen(find);
	do {
	    do {
		if ((sc = *s++) == 0)
		    return (0);
	    } while (sc != c);
	} while (strncmp(s, find, len) != 0);
	s--;
    }
    return ((char *)s);
}


/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/ia32/bitops.h
 * Description:   Bit mapipulation routines.
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
 * $Id: bitops.h,v 1.11 2006/08/18 13:13:11 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__IA32__BITOPS_H__
#define __AFTERBURN_WEDGE__INCLUDE__IA32__BITOPS_H__

#include INC_ARCH(types.h)

#ifdef CONFIG_SMP
#define SMP_PREFIX "lock; "
#else
#define SMP_PREFIX ""
#endif

INLINE word_t lsb( word_t w ) __attribute__ ((const));
INLINE word_t lsb( word_t w )
{
    word_t bitnum;
    __asm__ ("bsf %1, %0" : "=r"(bitnum) : "rm"(w));
    return bitnum;
}

INLINE word_t msb( word_t w ) __attribute__ ((const));
INLINE word_t msb( word_t w )
{
    word_t bitnum;
    __asm__ ("bsr %1, %0" : "=r"(bitnum) : "rm"(w));
    return bitnum;
}

template <typename T>
INLINE void bit_set_atomic( word_t bit, volatile T & word )
{
    __asm__ __volatile__ (SMP_PREFIX "btsl %1, %0" : "+m"(word) : "Ir"(bit));
}

template <typename T>
INLINE void bit_set( word_t bit, T & word )
{
    __asm__ __volatile__ ("btsl %1, %0" : "+m"(word) : "Ir"(bit));
}

template <typename T>
INLINE void bit_clear_atomic( word_t bit, volatile T & word )
{
    __asm__ __volatile__ (SMP_PREFIX "btrl %1, %0" : "+m"(word) : "Ir"(bit));
}

template <typename T>
INLINE void bit_clear( word_t bit, T & word )
{
    __asm__ __volatile__ ("btrl %1, %0" : "+m"(word) : "Ir"(bit));
}

template <typename T>
INLINE void bit_change_atomic( word_t bit, volatile T & word )
{
    __asm__ __volatile__ (SMP_PREFIX "btcl %1, %0" : "+m"(word) : "Ir"(bit));
}

template <typename T>
INLINE void bit_change( word_t bit, T & word )
{
    __asm__ __volatile__ (SMP_PREFIX "btcl %1, %0" : "+m"(word) : "Ir"(bit));
}

template <typename T>
INLINE word_t bit_test_and_set_atomic( word_t bit, volatile T & word )
{
    word_t old;
    __asm__ __volatile__ (SMP_PREFIX "btsl %2, %1 ; sbbl %0, %0"
	    : "=r"(old), "+m"(word) : "Ir"(bit) : "memory" );
    return old;
}

template <typename T>
INLINE word_t bit_test_and_set( word_t bit, T & word )
{
    word_t old;
    __asm__ __volatile__ ("btsl %2, %1 ; sbbl %0, %0"
	    : "=r"(old), "+m"(word) : "Ir"(bit) : "memory" );
    return old;
}

template <typename T>
INLINE word_t bit_test_and_clear_atomic( word_t bit, volatile T & word )
{
    word_t old = 0;
    __asm__ __volatile__ (SMP_PREFIX "btrl %2, %1 ; sbbl %0, %0"
	    : "=r"(old), "+m"(word) : "Ir"(bit) );
    return old;
}

template <typename T>

INLINE word_t bit_test_and_clear( word_t bit, T & word )
{
    word_t old;
    __asm__ __volatile__ ("btrl %2, %1 ; sbbl %0, %0"
	    : "=r"(old), "+m"(word) : "Ir"(bit) );
    return old;
}

INLINE word_t bit_allocate_atomic( volatile word_t base[], word_t bit_end )
    // Allocates the first zero bit found.  If it returns any value
    // greater than or equal to bit_end, then allocation failed.
{
    word_t word_count=0, tot_words=bit_end/sizeof(word_t);
    static const word_t zero = 0;
    word_t bit;

    if( !tot_words )
	tot_words++;

    while( word_count < tot_words ) {
	while( base[word_count] != ~zero ) {
	    // Due to SMP race conditions, operate on a *copy* of the word,
	    // since many bit search instructions are undefined if the
	    // word equals 0.
	    word_t word = base[word_count];
	    if( word == 0 )
		bit = 0;
	    else
		bit = lsb(~word);
	    if( !bit_test_and_set_atomic(bit, base[word_count]) )
		return bit + word_count*sizeof(word_t)*8;
	}
	word_count++;
    }

    return bit_end;
}

#endif	/* __AFTERBURN_WEDGE__INCLUDE__IA32__BITOPS_H__ */

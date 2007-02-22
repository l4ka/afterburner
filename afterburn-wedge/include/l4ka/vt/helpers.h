/*********************************************************************
 *
 * Copyright (C) 2005,  Karlsruhe University
 *
 * File path:     testvirt/page_pool.cc
 * Description:   Testing Pacifica Extensions of L4 application
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
 * $Id: pingpong.cc,v 1.56 2005/07/14 17:50:31 ud3 Exp $
 *
 ********************************************************************/

#ifndef __TESTVIRT__HELPERS_H__
#define __TESTVIRT__HELPERS_H__

#include <l4/types.h>
#include <l4/kip.h>

L4_Word_t roundUpNextLog2Sz (L4_Word_t sz);

L4_Word_t L4_IsInFpage (const L4_Fpage_t f, const L4_Word_t address);

/**
 * Rounds down an address to the next page boundary. If the address is a multiple
 * of PAGE_SIZE it is returned unmodified.
 */
#define TRUNC_PAGE(x)        (L4_Word_t)(((L4_Word_t)(x)) & ~(PAGE_SIZE - 1))


/**
 * Rounds down an address to the next page boundary. If the address is a multiple
 * of PAGE_SIZE it is returned unmodified.
 */
#define TRUNC_SUPERPAGE(x)        (L4_Word_t)(((L4_Word_t)(x)) & ~(SUPERPAGE_SIZE - 1L))


/**
 * Get the offset into a page for a certain address. If the address is a multipl
e of PAGE_SIZE, 0 will be returned.
*/
#define PAGE_OFFSET(x)       ((x) & (PAGE_SIZE-1) )


/**
 * Get the offset into a page for a certain address. If the address is a multipl
e of PAGE_SIZE, 0 will be returned.
*/
#define SUPERPAGE_OFFSET(x)       ((x) & (SUPERPAGE_SIZE-1L) )


/**
 * Rounds up an address to the next page boundary. If the address is a multiple
 * of PAGE_SIZE it is returned unmodified.
 */
#define ROUND_PAGE(x)        (L4_Word_t)((TRUNC_PAGE(x) == ((L4_Word_t)x)) ? (x) : (TRUNC_PAGE(x) + PAGE_SIZE))


/**
 * Rounds down an address to the next log2 boundary. If the address is a multiple
 * of the log2 it is returned unmodified.
*/
#define TRUNC_LOG2(addr,log2) \
	(L4_Word_t)(((L4_Word_t)(addr)) & (~0L << (log2)))

/**
 * Determines the offset of an address into a log2 boundary. If the address is a
 * multiple of the log2 boundary, 0 is returned.
 */
#define LOG2_OFFSET(addr,log2) \
	(L4_Word_t)((L4_Word_t)(addr) & ((1 << (log2)) - 1))


/**
 * Determines the offset of an address into a lin boundary. If the address is a
 * multiple of the lin boundary, 0 is returned.
 */
#define LIN_OFFSET(addr,lin) \
	(L4_Word_t)((L4_Word_t)(addr) & (lin) - 1)

/**
 * Rounds up an address to the next log2 boundary. If the address is a multiple
 * of the log2 boundary the address itself is returned.
 */
#define ROUND_LOG2(addr,log2) \
	(L4_Word_t)((TRUNC_LOG2((addr),(log2)) == ((L4_Word_t)(addr))) ? (addr) : (TRUNC_LOG2((addr),(log2)) + (1 << (log2))))


L4_INLINE bool is_fpage_conflict( L4_Fpage_t fp, L4_Word_t start, L4_Word_t end )
{
    if( (start < L4_Address(fp)) && (end > L4_Address(fp)) )
        return true;
    if( (start < (L4_Address(fp)+L4_Size(fp))) && (end > L4_Address(fp)) )
        return true;
    return false;
}


L4_INLINE bool strcmp (char *s, char *t)
{
    for ( ; *s == *t; s++, t++)
    {
	if (*s == '\0')
	    return true;
    }
    return false;
}


L4_INLINE L4_Bool_t L4_HasFeature (L4_KernelInterfacePage_t *kip, char *feature)
{
    L4_Word_t i = 0;

    if (feature == 0)
	return false;

    while (true)
    {
	char * kip_feature = L4_Feature(kip, i);

	if (kip_feature == 0)
	    return false;

	if (strcmp (feature, kip_feature))
	    return true;

	i++;
    }
    return false;
}

#define L4_ERRORCODE_TO     1
#define L4_ERRORCODE_NEP    2
#define L4_ERRORCODE_CANCEL 3

typedef struct {
    union {
	L4_Word_t raw;
	struct {
	    L4_Word_t p		:  1;
	    L4_Word_t e		:  3;
	    L4_Word_t error	: 28;
	} X;
    };
} L4_ErrorCode_t;

#endif /* __TESTVIRT__HELPERS_H__ */

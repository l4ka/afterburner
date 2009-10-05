/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     l4ka-basics.h
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
 ********************************************************************/
#ifndef __BASICS_H__
#define __BASICS_H__

#include <ia32/msr.h>

extern bool sigma0_request_region( L4_Word_t start, L4_Word_t end );
extern bool kip_conflict( L4_Word_t start, L4_Word_t size, L4_Word_t *next );
extern bool l4_has_feature( char *feature_name );

L4_INLINE bool
is_fpage_conflict( L4_Fpage_t fp, L4_Word_t start, L4_Word_t end )
{
    if( (start < L4_Address(fp)) && (end > L4_Address(fp)) )
        return true;
    if( (start < (L4_Address(fp)+L4_Size(fp))) && (end > L4_Address(fp)) )
        return true;
    return false;
}

L4_INLINE bool
is_region_conflict( L4_Word_t start1, L4_Word_t end1,
	            L4_Word_t start2, L4_Word_t end2 )
{
    if( (start2 < start1) && (end2 > start1) )
	return true;
    if( (start2 < end1) && (end2 > start1) )
	return true;
    return false;
}

extern inline bool is_fpage_intersect( L4_Fpage_t fp1, L4_Fpage_t fp2 )
{
    L4_Word_t start1 = L4_Address(fp1);
    L4_Word_t end1 = start1 + L4_Size(fp1);
    L4_Word_t start2 = L4_Address(fp2);
    L4_Word_t end2 = start2 + L4_Size(fp2);

    if( (start1 < start2) && (end1 > end2) )
	return true;
    if( (start1 < start2) && (end1 > start2) )
	return true;
    if( (start1 < end2) && (end1 > end2) )
	return true;
    return false;
}

typedef union {
    L4_Word64_t raw;
    L4_Word_t   x[2];
} u64_typed_t;


L4_INLINE int rand()
{   
    static unsigned int s=1;
    
    s = 48271 * (s % 44488) - 3399 * (L4_Word32_t) (s / 44488);
    
    if (s < 0)
	s += ((1U<<31) -1);

    return s;
}



#endif	/* __BASICS_H__ */

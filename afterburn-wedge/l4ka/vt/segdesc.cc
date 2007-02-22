/*********************************************************************
 *
 * Copyright (C) 2002, 2006,  Karlsruhe University
 *
 * File path:     arch/ia32/segdesc.h
 * Description:   IA32 Segment Descriptor
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
 * $Id: segdesc.h,v 1.5 2003/09/24 19:04:27 skoglund Exp $
 *
 ********************************************************************/

#include INC_WEDGE(vt/segdesc.h)


void ia32_segdesc_t::set_seg(L4_Word32_t base, L4_Word32_t limit,
			     int dpl, segtype_e type)
{
    if (limit > ( 1 << 20))
    {
	x.d.limit_low  = (limit >> 12) & 0xFFFF;
	x.d.limit_high = (limit >> 28) & 0xF;
	x.d.g = 1;	/* 4K granularity	*/
    }
    else
    {
	x.d.limit_low  =  limit        & 0xFFFF;
	x.d.limit_high = (limit >> 16) & 0xFF;
	x.d.g = 0;	/* 1B granularity	*/
    }

    x.d.base_low   = base & 0xFFFFFF;
    x.d.base_high  = (base >> 24) & 0xFF;
    x.d.type = type;
    x.d.dpl = dpl;

    /* default fields */
    x.d.p = 1;	/* present		*/
    x.d.d = 1;	/* 32-bit segment	*/
    x.d.s = 1;	/* non-system segment	*/

    /* unused fields */
    x.d.avl = 0;
}


void ia32_segdesc_t::set_sys(L4_Word32_t base, L4_Word32_t limit,
			     int dpl, segtype_e type)
{
    x.d.limit_low  =  limit        & 0xFFFF;
    x.d.limit_high = (limit >> 16) & 0xFF;
    x.d.base_low   = base	   & 0xFFFFFF;
    x.d.base_high  = (base >> 24)  & 0xFF;
    x.d.type = type;
    x.d.dpl = dpl;

    /* default fields */
    x.d.p = 1;	/* present		*/
    x.d.g = 0;	/* byte granularity	*/
    x.d.d = 0;	/* 32-bit segment	*/
    x.d.s = 0;	/* non-system segment	*/

    /* unused fields */
    x.d.avl = 0;
}

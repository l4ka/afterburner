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
#ifndef __TESTVIRT__SEGDESC_H__
#define __TESTVIRT__SEGDESC_H__

#include <l4/types.h>

class ia32_segdesc_t
{
public:
    enum segtype_e
    {
	inv	= 0x0,
	code	= 0xa,
	code_a	= 0xb,
	data	= 0x2,
	data_a	= 0x3,
	tss	= 0x9
    };

    void set_seg(L4_Word32_t base, L4_Word32_t limit, int dpl, segtype_e type);
    void set_sys(L4_Word32_t base, L4_Word32_t limit, int dpl, segtype_e type);

    L4_Word_t get_base (void)
	{ return  (x.d.base_high << 24) | x.d.base_low; };

public:
    union {
	L4_Word32_t raw[2];
	L4_Word8_t  raw8[4];
	struct {
	    L4_Word32_t limit_low	: 16;
	    L4_Word32_t base_low	: 24 __attribute__((packed));
	    L4_Word32_t type		:  4;
	    L4_Word32_t s		:  1;
	    L4_Word32_t dpl		:  2;
	    L4_Word32_t p		:  1;
	    L4_Word32_t limit_high	:  4;
	    L4_Word32_t avl		:  2;
	    L4_Word32_t d		:  1;
	    L4_Word32_t g		:  1;
	    L4_Word32_t base_high	:  8;
	} d __attribute__((packed));
    } x;
};


// This is how GRUB initzializes the Segments.
//     gdt[1].x.raw8[0] = 0xFF;
//     gdt[1].x.raw8[1] = 0xFF;
//     gdt[1].x.raw8[2] = 0x00;
//     gdt[1].x.raw8[3] = 0x00;
//     gdt[1].x.raw8[4] = 0x00;
//     gdt[1].x.raw8[5] = 0x9A;
//     gdt[1].x.raw8[6] = 0xCF;
//     gdt[1].x.raw8[7] = 0x00;
//     gdt[2].x.raw8[0] = 0xFF;
//     gdt[2].x.raw8[1] = 0xFF;
//     gdt[2].x.raw8[2] = 0x00;
//     gdt[2].x.raw8[3] = 0x00;
//     gdt[2].x.raw8[4] = 0x00;
//     gdt[2].x.raw8[5] = 0x92;
//     gdt[2].x.raw8[6] = 0xCF;
//     gdt[2].x.raw8[7] = 0x00;


#endif /* !__TESTIVRT__SEGDESC_H__ */

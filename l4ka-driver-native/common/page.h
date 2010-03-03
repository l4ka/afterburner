/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     common/page.h
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
#ifndef __COMMON__PAGE_H__
#define __COMMON__PAGE_H__

#include "ia32/types.h"

class arch_i386_t {
};

class arch_ia64_t {
};

typedef arch_ia32_t arch_t;



template <class arch>
struct page_traits_t {
};

template <>
struct page_traits_t<arch_ia32_t> {
public:
    static const word_t page_bits = 12;
    static const word_t page_size = 1 << page_bits;
    static const word_t page_offset_mask = page_size - 1;
    static const word_t page_mask = ~page_offset_mask;

    static const word_t superpage_bits = 22;
    static const word_t superpage_size = 1 << superpage_bits;
    static const word_t superpage_offset_mask = superpage_size - 1;
    static const word_t superpage_mask = ~superpage_offset_mask;
};

static const word_t page_bits = page_traits_t<arch_t>::page_bits;
static const word_t page_size = page_traits_t<arch_t>::page_size;

#endif	/* __COMMON__PAGE_H__ */

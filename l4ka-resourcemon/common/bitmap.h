/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/bitmap.h
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
#ifndef __HYPERVISOR__INCLUDE__BITMAP_H__
#define __HYPERVISOR__INCLUDE__BITMAP_H__

template <L4_Word32_t _size> class bitmap_t
{
private:
    L4_Word_t bitmap[ _size / sizeof(L4_Word_t) ];

    L4_Word32_t bit_word( L4_Word32_t bit )
	{ return bit / sizeof(L4_Word_t); }

    L4_Word32_t bit_word_offset( L4_Word32_t bit )
	{ return bit % sizeof(L4_Word_t); }

public:

    bool set_bit( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
	this->bitmap[ bit_word(bit) ] |= (1 << bit_word_offset(bit));
	return true;
    }

    bool clr_bit( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
	this->bitmap[ bit_word(bit) ] &= ~(1 << bit_word_offset(bit));
	return true;
    }

    bool is_bit_set( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
	L4_Word32_t bit_value = this->bitmap[ bit_word(bit) ] & 
	    (1 << bit_word_offset(bit));
	return (bit_value != 0);
    }
};

#endif	/* __HYPERVISOR__INCLUDE__BITMAP_H__ */

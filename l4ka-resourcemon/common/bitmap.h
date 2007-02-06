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
#ifndef __HOME__STOESS__RESOURCEMON__COMMON__BITMAP_H__
#define __HOME__STOESS__RESOURCEMON__COMMON__BITMAP_H__

#include <common/ia32/bitops.h>
#include <common/debug.h>


template <L4_Word32_t _size> class bitmap_t
{
private:
    
    static const L4_Word_t bits_per_word = 8 * sizeof(L4_Word_t);
    static const L4_Word_t bitmap_size = _size ? _size / bits_per_word : 1;
    
    L4_Word_t bitmap[ bitmap_size ];


    L4_Word32_t word( const L4_Word32_t bit )
	{ return bit / bits_per_word ; }

    L4_Word32_t word_offset( const L4_Word32_t bit )
	{ return bit % bits_per_word; }
    
    L4_Word32_t mask( const L4_Word32_t bit )
	{ return ((1 << bit) - 1); }


public:

    bool set( L4_Word32_t bit )
    {
	if( bit >= _size )
	{
	    ASSERT(false);
	    return false;
	}
	bit_set( word_offset(bit), this->bitmap[ word(bit) ] );
	return true;
    }

    bool clear( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
	bit_clear( word_offset(bit), this->bitmap[ word(bit) ] );
	return true;
    }
    
    bool test_and_clear_atomic( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
	return bit_test_and_clear_atomic( word_offset(bit), 
		this->bitmap[ word(bit) ] );
    }

    
    bool is_bit_set( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
	L4_Word32_t bit_value = this->bitmap[ word(bit) ] & 
	    (1 << word_offset(bit));
	return (bit_value != 0);
    }
    

    bool find_first_bit(L4_Word_t &bit = 0)
    {
	if (word_offset(bit) &&
	    this->bitmap[ word(bit) ] & mask(word_offset(bit)))
	{
	    bit += lsb(this->bitmap[word(bit)] & mask(word_offset(bit)));
	    return true;
	}
	
	for (bit += bits_per_word - word_offset(bit); bit < _size ; bit += bits_per_word)
	    if (this->bitmap[ word(bit) ])
	    {
		bit += lsb(this->bitmap[word(bit)]);
		return true;
	    }
	return false;
    }    
    

};

#endif /* !__HOME__STOESS__RESOURCEMON__COMMON__BITMAP_H__ */

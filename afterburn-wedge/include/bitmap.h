/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     bitmap.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <l4/types.h>
#include INC_ARCH(bitops.h)


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
    
    const L4_Word32_t mask( const L4_Word32_t bit )
	{ return ((1 << bit) - 1); }


public:

    bool set( L4_Word32_t bit )
    {
	if( bit >= _size )
	    return false;
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
    

    bool find_msb(L4_Word_t &bit = _size)
    {
	if (word_offset(bit))
	{
	    if (this->bitmap[ word(bit) ] & mask(word_offset(bit)))
	    {
		bit -= word_offset(bit) - msb(this->bitmap[word(bit)] & mask(word_offset(bit)));
		return true;
	    }
	    bit -= word_offset(bit);
	}
	
	for (bit -= bits_per_word; bit != -bits_per_word ; bit -= bits_per_word)
	    if (this->bitmap[ word(bit) ])
	    {
		bit += msb(this->bitmap[word(bit)]);	
		return true;
	    }
	return false;
    }    
    

};

#endif /* !__BITMAP_H__ */

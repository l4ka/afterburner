#ifndef L4KAVT_COMMON_BITFIELD_H__
#define L4KAVT_COMMON_BITFIELD_H__

#include <l4/types.h>
#include <ia32/types.h>
#include INC_WEDGE(debug.h)
#include INC_WEDGE(console.h)


template<u32_t size> class bitfield_t
{
public:
    // Size.
    static const L4_Word_t NUM_WORDS = (size + sizeof(L4_Word_t)*8 - 1) / (sizeof(L4_Word_t)*8);
public:
    bitfield_t<size> (void);
    bitfield_t<size> (const bitfield_t<size> &bf);

    static const L4_Word_t INV_IDX = ~0UL;
    
    // Zeroed Bitfield.
    bool clear (void);
    bool init (void);

    // Initialize word-by-word.
    bool init (L4_Word_t index, L4_Word_t val);

    // Initialize with an raw array.
    bool init (L4_Word_t val[NUM_WORDS]);

    L4_Word_t getFirstUsableBit (void) const;

    void setBit (L4_Word_t index);
    void unsetBit (L4_Word_t index);

    bitfield_t<size>& operator<< (L4_Word_t index);
    void orField (bitfield_t<size>& bitfield);

    // Is the Index valid.
    bool isIn (L4_Word_t index) const;

    // Is the Bit Set.
    bool isSet (L4_Word_t index) const;

    // Size in L4_Word_ts
    L4_Word_t getSizeInWords (void) const;
    L4_Word_t getMaxSizeInWords (void) const;

    // How many Bits the Field can hold.
    L4_Word_t getSizeInBits (void) const;
    L4_Word_t getMaxSizeInBits (void) const;

    L4_Word_t getInvIdx (void) const
	{ return bitfield_t<size>::INV_IDX; }

    // For the first Bit use an invalid position.
    L4_Word_t getNextSetBit (L4_Word_t curr_pos) const;

    // Get the raw Bitfield.
    L4_Word_t getRaw (L4_Word_t word_no) const;

    void dump (void);

private:
    static const L4_Word_t BITS_PER_WORD = sizeof(L4_Word_t) * 8;

    // The Bitfield itself.
    L4_Word_t raw[NUM_WORDS];
};

#if defined(L4_ARCH_IA32)
# include INC_WEDGE(vt/ia32_bitfield.h)
#else
# error Define Architecture.
#endif

template<u32_t size>
INLINE bitfield_t<size>::bitfield_t (void)
{
    this->init();
}


template<u32_t size>
INLINE bitfield_t<size>::bitfield_t (const bitfield_t<size> &bf)
{
    for (L4_Word_t i=0; i < NUM_WORDS; i++)
    {
	this->raw[i] = bf.getRaw(i);
    }
}


template<u32_t size>
INLINE bool bitfield_t<size>::clear (void)
{
    return this->init();
}


template<u32_t size>
INLINE bool bitfield_t<size>::init (void)
{
    // Clear the Bitfield.
    for (L4_Word_t i=0; i < NUM_WORDS; i++)
	raw[i] = 0;
    return true;
}


template<u32_t size>
INLINE bool bitfield_t<size>::init (L4_Word_t index, L4_Word_t val)
{
    ASSERT (index < NUM_WORDS);
    raw[index] = val;
    return true;
}


template<u32_t size>
INLINE bool bitfield_t<size>::init (L4_Word_t val[NUM_WORDS])
{
    for (L4_Word_t i=0; i < NUM_WORDS; i++)
	raw[i] = val[i];
    return true;
}


template<u32_t size>
INLINE L4_Word_t bitfield_t<size>::getFirstUsableBit (void) const
{
    return 0;
}


template<u32_t size>
INLINE bitfield_t<size>& bitfield_t<size>::operator<< (L4_Word_t index)
{
    this->setBit (index);
    return *this;
}

template<u32_t size>
INLINE void bitfield_t<size>::orField (bitfield_t<size> &bitfield)
{
    for (L4_Word_t i=0; i < NUM_WORDS; i++)
	this->raw[i] |= bitfield.getRaw(i);
}


template<u32_t size>
INLINE bool bitfield_t<size>::isIn (L4_Word_t index) const
{
    return (0 <= index) && (index < this->getMaxSizeInBits());
}


template<u32_t size>
INLINE	L4_Word_t bitfield_t<size>::getSizeInWords (void) const
{
    return bitfield_t<size>::NUM_WORDS;
}


template<u32_t size>
INLINE	L4_Word_t bitfield_t<size>::getMaxSizeInWords (void) const
{
    return bitfield_t<size>::NUM_WORDS;
}


template<u32_t size>
INLINE	L4_Word_t bitfield_t<size>::getSizeInBits (void) const
{
     return size;
}


template<u32_t size>
INLINE	L4_Word_t bitfield_t<size>::getMaxSizeInBits (void) const
{
    return bitfield_t<size>::NUM_WORDS * BITS_PER_WORD;
}


template<u32_t size>
INLINE L4_Word_t bitfield_t<size>::getNextSetBit (L4_Word_t curr_pos=bitfield_t<size>::INV_IDX) const
{
    // If invalid position is requested; this means start from 0.
    if (!this->isIn(curr_pos))
	curr_pos = 0;

    while (!this->isSet(curr_pos))
    {
	curr_pos++;
	if (curr_pos >= this->getMaxSizeInBits())
	    return bitfield_t<size>::INV_IDX;
    }

    // This Bit is set.
    return curr_pos;
}


template<u32_t size>
INLINE L4_Word_t bitfield_t<size>::getRaw (L4_Word_t word_no) const
{
    ASSERT (word_no < this->getMaxSizeInWords());
    return this->raw[word_no];
}


template<u32_t size>
INLINE void bitfield_t<size>::dump (void)
{
    con << "Dumping Bitfield (size: " << getMaxSizeInBits() << " bits"
	<< " | " << getSizeInWords() << " words)";

    for (L4_Word_t i=0 ; i < this->getMaxSizeInWords(); i++)
    {
	if (i % 10 == 0)
	    con << "\n " << i;

	con << "  " << (void*)this->raw[i];

    }

    con << "\n";
}


#endif /* L4KAVT_COMMON_BITFIELD_H__ */

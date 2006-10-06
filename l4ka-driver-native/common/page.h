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


#include <common/valloc.h>
#include <common/console.h>
#include <common/debug.h>

valloc_t valloc;

word_t valloc_t::alloc_aligned( log2size_t log2size )
{
    extern u8_t _end[];

    word_t size = 1 << log2size;
    end_vaddr -= size;
    end_vaddr &= ~(size-1);

    ASSERT( end_vaddr > (word_t)_end );
    return end_vaddr;
}



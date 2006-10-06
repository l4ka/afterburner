#ifndef __COMMON__VADDR_H__
#define __COMMON__VADDR_H__

typedef unsigned log2size_t;

class valloc_t
{
public:
    static const word_t max_vaddr = cfg_end_vaddr;

    valloc_t() { end_vaddr = max_vaddr; }

    word_t alloc_aligned( log2size_t log2size );

protected:
    word_t end_vaddr;
};

INLINE valloc_t * get_valloc()
{
    extern valloc_t valloc;
    return &valloc;
}


#endif	/* __COMMON__VADDR_H__ */

#ifndef __TESTVIRT__MEM_H__
#define __TESTVIRT__MEM_H__

#include <l4/types.h>

class mem_t
{
    public:
	virtual bool memcpy(void *dest, void *src, L4_Word_t len) = 0;
	virtual bool memset(L4_Word_t *dest, L4_Word_t val, L4_Word_t len) = 0;
};

#endif /* __TESTVIRT__MEM_H__ */

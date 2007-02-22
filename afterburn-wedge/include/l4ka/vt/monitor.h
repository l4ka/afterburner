#ifndef L4KA_VT_MONITOR_H_
#define L4KA_VT_MONITOR_H_

#include INC_WEDGE(vt/ia32.h)

extern void monitor_loop( void );

INLINE word_t get_map_addr( word_t addr )
{
    // TODO: prevent overlapping
    if( addr >= 0xbc000000 ) {
	return addr - 0xbc000000 + 0x40000000;
    } else {
	return addr;
    }
}

#endif // L4KA_VT_MONITOR_H_

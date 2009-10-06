#ifndef __DEVICE__I8254_H__
#define __DEVICE__I8254_H__

#include INC_ARCH(bitops.h)
#include INC_ARCH(cycles.h)
#include INC_WEDGE(intmath.h)

void pit_handle_timer_interrupt();
u32_t pit_get_remaining_usecs();
void pit_init();


#endif /*  __DEVICE__I8254_H__ */

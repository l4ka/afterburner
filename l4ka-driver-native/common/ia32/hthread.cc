
#include <common/hthread.h>

void hthread_t::arch_prepare_exreg( L4_Word_t &sp, L4_Word_t &ip )
{
    ip = (L4_Word_t)hthread_t::self_start;
}


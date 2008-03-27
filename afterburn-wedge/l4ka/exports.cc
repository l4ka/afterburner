/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/exports.cc
 * Description:   L4 exports for use by L4-aware code in the guest OS.
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

#include <debug.h>
#include <burn_symbols.h>
#include <stdarg.h>	/* for va_list, ... comes with gcc */
#include <console.h>
#include INC_ARCH(intlogic.h)

#include INC_WEDGE(message.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(dspace.h)



pgent_t *guest_pdir_master = NULL;

typedef void (*l4ka_wedge_thread_func_t)( void * );

static void thread_create_trampoline( void *param, l4thread_t *l4thread )
{
    l4ka_wedge_thread_func_t func = (l4ka_wedge_thread_func_t)param;

    func( l4thread->get_tlocal_data() );
}

extern "C" L4_ThreadId_t
l4ka_wedge_thread_create(
	L4_Word_t stack_bottom, L4_Word_t stack_size, L4_Word_t prio,
	l4ka_wedge_thread_func_t thread_func,
	void *tlocal_data, unsigned tlocal_size )
{
    vcpu_t &vcpu = get_vcpu();
    L4_ThreadId_t monitor_tid = vcpu.monitor_gtid;
    
    l4thread_t *l4thread =
	get_l4thread_manager()->create_thread( &vcpu, stack_bottom, stack_size,
                prio, thread_create_trampoline, monitor_tid,
		(void *)thread_func, tlocal_data, tlocal_size);

   if( !l4thread )
	return L4_nilthread;

   l4thread->start();

    return l4thread->get_global_tid();
}

extern "C" void l4ka_wedge_thread_delete( L4_ThreadId_t tid )
{
    get_l4thread_manager()->terminate_thread( tid );
}

extern "C" L4_Word_t l4ka_wedge_get_irq_prio( void )
{
    return get_vcpu().get_vcpu_max_prio() + CONFIG_PRIO_DELTA_IRQ;
}

extern "C" void l4ka_wedge_raise_irq( L4_Word_t irq )
{
    get_intlogic().raise_irq( irq );
}

extern "C" L4_Word_t l4ka_wedge_phys_to_bus( L4_Word_t paddr )
{
    return backend_phys_to_dma_hook( paddr );
}

extern "C" L4_Word_t l4ka_wedge_bus_to_phys( L4_Word_t dma_addr )
{
    return backend_dma_to_phys_hook( dma_addr );
}

extern "C" void l4ka_wedge_add_virtual_irq( L4_Word_t irq )
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    get_intlogic().add_virtual_hwirq( irq );
    get_intlogic().clear_hwirq_squash( irq );
#endif
}

extern "C" void l4ka_wedge_add_dspace_handler( dspace_pfault_handler_t handler, void *data )
{
    dspace_handlers.add_dspace_handler( handler, data );
}

extern "C" void l4ka_wedge_declare_pdir_master( pgent_t * pdir_master )
{
    guest_pdir_master = pdir_master;
}

extern int do_dbg_printf(const char* format_p, va_list args);
extern void do_trace_printf(const word_t id, const word_t type,
			   const char* format_p, va_list args);

extern "C" void l4ka_wedge_debug_printf( L4_Word_t id, L4_Word_t level, const char *format, ...)
{
    debug_id_t debug_id(id, level);
    va_list args;   
    word_t type = max((word_t) level, (word_t) DBG_LEVEL) - DBG_LEVEL;
    type = 1 << type;
    
    va_start(args, format);

    if(level < DBG_LEVEL)
	do_dbg_printf(format, args);
    if (level < TRACE_LEVEL && l4_tracebuffer_enabled)	
	do_trace_printf(id + L4_TRACEBUFFER_USERID_START, type, format, args);

    va_end(args);

}

extern "C" L4_MsgTag_t l4ka_wedge_send_virtual_irq( L4_Word_t virq, L4_ThreadId_t tid, L4_Time_t timeout)
{
    msg_virq_build(virq, L4_nilthread);
    return backend_notify_thread(tid, timeout);
}

extern "C" L4_MsgTag_t l4ka_wedge_notify_thread(L4_ThreadId_t tid, L4_Time_t timeout)
{
    return backend_notify_thread(tid, timeout);
}

DECLARE_BURN_SYMBOL(l4ka_wedge_thread_create);
DECLARE_BURN_SYMBOL(l4ka_wedge_thread_delete);
DECLARE_BURN_SYMBOL(l4ka_wedge_get_irq_prio);
DECLARE_BURN_SYMBOL(l4ka_wedge_raise_irq);
DECLARE_BURN_SYMBOL(l4ka_wedge_phys_to_bus);
DECLARE_BURN_SYMBOL(l4ka_wedge_bus_to_phys);
DECLARE_BURN_SYMBOL(l4ka_wedge_add_virtual_irq);
DECLARE_BURN_SYMBOL(l4ka_wedge_send_virtual_irq);
DECLARE_BURN_SYMBOL(l4ka_wedge_add_dspace_handler);
DECLARE_BURN_SYMBOL(l4ka_wedge_declare_pdir_master);
DECLARE_BURN_SYMBOL(l4ka_wedge_notify_thread);
DECLARE_BURN_SYMBOL(l4ka_wedge_debug_printf);
DECLARE_BURN_SYMBOL(resourcemon_shared);

extern void * __L4_Ipc;
DECLARE_BURN_SYMBOL(__L4_Ipc);


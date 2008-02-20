/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-driver-reuse/kernel/glue/wedge.h
 * Description:	Declarations for the functions and data types that
 * 		we dynamically link against in the L4Ka wedge.
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
#ifndef __L4KA_DRIVER_REUSE__KERNEL__GLUE__WEDGE_H__
#define __L4KA_DRIVER_REUSE__KERNEL__GLUE__WEDGE_H__

#include <l4/types.h>

#include <linux/kernel.h>
#include <linux/sched.h>

#include "resourcemon_idl_client.h"

#define BURN_WEDGE_MODULE_INTERFACE_VERSION	1


typedef struct {
    L4_Word_t version;
} burn_wedge_header_t;

typedef int (*dspace_pfault_handler_t)(L4_Word_t fault_addr, L4_Word_t *ip, L4_Fpage_t *map_fp, void *data);

typedef struct
{
    L4_ThreadId_t monitor_ltid;		// 0
    L4_ThreadId_t monitor_gtid;		// 4
    L4_ThreadId_t irq_ltid;		// 8
    L4_ThreadId_t irq_gtid;		// 12
    L4_ThreadId_t main_ltid;		// 16
    L4_ThreadId_t main_gtid;		// 20

    volatile int dispatch_ipc;		// 24
    volatile L4_Word_t dispatch_ipc_nr;	// 28

    struct {
	L4_Word_t flags;		// 32
    } cpu;
} vcpu_t;

static inline __attribute__((const)) vcpu_t *get_vcpu( void )
{
#if 0
    vcpu_t *vcpu = (vcpu_t *)L4_UserDefinedHandled();
    return vcpu;
#else
    extern vcpu_t vcpu;
    return &vcpu;
#endif
}

extern L4_MsgTag_t l4ka_wedge_notify_thread( L4_ThreadId_t tid, L4_Time_t timeout);
extern void l4ka_wedge_debug_printf( L4_Word_t id, L4_Word_t level, const char *format, ...);

extern volatile IResourcemon_shared_t resourcemon_shared;

extern L4_ThreadId_t l4ka_wedge_thread_create( L4_Word_t stack_bottom,
	L4_Word_t stack_size, L4_Word_t prio, 
	void (*thread_func)(void *), void *tlocal_data, unsigned tlocal_size );
extern void l4ka_wedge_thread_delete( L4_ThreadId_t tid );

extern L4_Word_t l4ka_wedge_get_irq_prio( void );
extern void l4ka_wedge_raise_irq( L4_Word_t irq );
extern void l4ka_wedge_add_virtual_irq( L4_Word_t irq );

extern L4_Word_t l4ka_wedge_phys_to_bus( L4_Word_t paddr );
extern L4_Word_t l4ka_wedge_bus_to_phys( L4_Word_t bus_addr );


static inline int vcpu_in_dispatch_ipc( void )
	{ return get_vcpu()->dispatch_ipc; }

static inline int vcpu_interrupts_enabled( void )
	{ return (get_vcpu()->cpu.flags >> 9) & 1; }

extern void l4ka_wedge_add_dspace_handler( dspace_pfault_handler_t handler, void * data );

extern void l4ka_wedge_declare_pdir_master( pgd_t *pgd );

#endif /* __L4KA_DRIVER_REUSE__KERNEL__GLUE__WEDGE_H__ */

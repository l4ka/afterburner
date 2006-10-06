/*********************************************************************
 *
 * Copyright (C) 2004-2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/vmirq.h
 * Description:	
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
#ifndef __L4KA_DRIVERS__GLUE__VMIRQ_H__
#define __L4KA_DRIVERS__GLUE__VMIRQ_H__

#include <l4/types.h>

#include <linux/version.h>
#include <asm/system.h>

#include "l4linux_idl_client.h"
#include "wedge.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
typedef void irqreturn_t;
#define IRQ_HANDLED
#endif

extern inline L4_ThreadId_t L4VM_linux_irq_thread( L4_Word_t l4_cpu )
{
    return get_vcpu()->irq_gtid;
}

extern inline L4_ThreadId_t L4VM_linux_main_thread( L4_Word_t l4_cpu )
{
    return get_vcpu()->main_gtid;
}

typedef struct L4VM_irq
{
    L4_ThreadId_t my_irq_tid;
    volatile int pending;
    volatile L4_Word_t status;
    volatile L4_Word_t mask;
} L4VM_irq_t;

extern inline void L4VM_irq_init( L4VM_irq_t *irq )
{
    irq->my_irq_tid = L4VM_linux_irq_thread( L4_ProcessorNo() );
    irq->pending = 0;
    irq->status = 0;
    irq->mask = ~0UL;
}

extern inline int L4VM_irq_deliver_prepare( 
	L4VM_irq_t *irq,
	L4_Word_t linux_irq_no,
	L4_Word_t irq_flags )
{
    irq->status |= irq_flags;

    if( ((irq->status & irq->mask) != 0) && !irq->pending )
    {
	L4_MsgTag_t msgtag;

	irq->pending = 1;

	msgtag.raw = 0;
	L4_Set_Label( &msgtag, linux_irq_no );
	L4_Set_MsgTag( msgtag );

	return 1;
    }

    return 0;
}

extern inline void L4VM_irq_deliver( 
	L4VM_irq_t *irq,
	L4_Word_t linux_irq_no,
	L4_Word_t irq_flags )
{
    irq->status |= irq_flags;

    if( ((irq->status & irq->mask) != 0) && !irq->pending )
    {
	CORBA_Environment ipc_env = idl4_default_environment;

	irq->pending = 1;

	ILinux_raise_irq( irq->my_irq_tid, linux_irq_no, &ipc_env );

	if( ipc_env._major != CORBA_NO_EXCEPTION )
	    CORBA_exception_free(&ipc_env);
    }
}

extern inline L4_Word_t L4VM_irq_status_reset( volatile L4_Word_t *status )
{
    L4_Word_t events;

    do {
	events = *status;
    } while( cmpxchg(status, events, 0) != events );

    return events;
}

#endif	/* __L4KA_DRIVERS__GLUE__VMIRQ_H__ */

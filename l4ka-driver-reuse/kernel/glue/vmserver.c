/*********************************************************************
 *
 * Copyright (C) 2004-2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/vmmemory.c
 * Description:	Routines common for L4 server threads in the Linux kernel.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>

#include "hypervisor_idl_client.h"
#include "vmserver.h"
#include "wedge.h"

EXPORT_SYMBOL(L4VM_server_cmd_allocate);
EXPORT_SYMBOL(L4VM_server_cmd_dispatcher);
EXPORT_SYMBOL(L4VM_server_cmd_init);
EXPORT_SYMBOL(L4VM_server_register_location);
EXPORT_SYMBOL(L4VM_server_locate);


L4_Word16_t L4VM_server_cmd_allocate(
	L4VM_server_cmd_ring_t *ring,
	L4_Word_t irq_flags,
	L4VM_irq_t *irq,
	L4_Word_t linux_irq_no )
{
    L4_Word16_t cmd_idx = ring->next_free;
    L4VM_server_cmd_t *cmd = &ring->cmds[ cmd_idx ];

    // Wait until the command is available for use.  We wait for an
    // IPC from *any* thread, so confirm that we actually have
    // a free cmd slot when we wake.
    while( cmd->handler )
    {
	L4_ThreadId_t to_tid, from_tid;

	// 1. Tell the handler that it needs to wake the server thread.
	ring->wake_server = 1;

	// 2. Prepare for IRQ delivery.
	if( L4VM_irq_deliver_prepare(irq, linux_irq_no, irq_flags) )
	    to_tid = irq->my_irq_tid;
	else
	    to_tid = L4_nilthread;

	// 3. Deliver the IRQ, and wait for reactivation.
	L4_ReplyWait_Timeout( to_tid, L4_Never, &from_tid );
	// Ignore IPC errors and IPC from unknown clients.
	// We restart the loop, which will correct for errors.
    }

    // Move to the next command.
    ring->next_free = (ring->next_free + 1) % L4VM_SERVER_CMD_RING_LEN;

    return cmd_idx;
}

void L4VM_server_cmd_dispatcher( 
	L4VM_server_cmd_ring_t *cmd_ring,
	void *data,
	L4_ThreadId_t server_tid)
{
    while( 1 )
    {
	L4VM_server_cmd_t *cmd = &cmd_ring->cmds[ cmd_ring->next_cmd ];
	if( !cmd->handler )
	    break;

	// Dispatch the command entry.
	cmd->handler( cmd, data );

	// Release the command entry and get the next command.
	cmd->handler = NULL;
	cmd_ring->next_cmd = (cmd_ring->next_cmd + 1) % L4VM_SERVER_CMD_RING_LEN;
    }

    if( cmd_ring->wake_server )
    {
	L4_MsgTag_t msgtag;
	
	cmd_ring->wake_server = 0;

	msgtag.raw = 0;
	L4_Set_MsgTag( msgtag );
	L4_Reply( server_tid );
    }
}

void
L4VM_server_cmd_init( L4VM_server_cmd_ring_t *cmd_ring )
{
    L4_Word16_t idx;

    cmd_ring->next_free = 0;
    cmd_ring->next_cmd = 0;
    cmd_ring->wake_server = 0;

    for( idx = 0; idx < L4VM_SERVER_CMD_RING_LEN; idx++ )
    {
	cmd_ring->cmds[idx].handler = NULL;
	cmd_ring->cmds[idx].reply_to_tid = L4_nilthread;
    }
}


int L4VM_server_register_location( 
	guid_t guid,
	L4_ThreadId_t tid )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IHypervisor_register_interface( 
	    resourcemon_shared.cpu[L4_ProcessorNo()].locator_tid, 
	    guid, &tid, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -ENODEV;
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -EIO;
    }

    return 0;
}

int L4VM_server_locate( 
	guid_t guid,
	L4_ThreadId_t *server_tid)
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
               
    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IHypervisor_query_interface( 
	    resourcemon_shared.cpu[L4_ProcessorNo()].locator_tid, 
	    guid, server_tid, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -ENODEV;
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -EIO;
    }

    return 0;
}


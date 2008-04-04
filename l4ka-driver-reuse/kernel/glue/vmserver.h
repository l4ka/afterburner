/*********************************************************************
 *
 * Copyright (C) 2004-2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/vmmemory.h
 * Description:	Common data structures for L4 servers implemented in
 * 		the Linux kernel.
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
#ifndef __L4KA_DRIVERS__GLUE__VMSERVER_H__
#define __L4KA_DRIVERS__GLUE__VMSERVER_H__

#include <l4/types.h>

#include "wedge.h"
#include "resourcemon_idl_client.h"

/* this is required to get things compiled with gcc 3.3 */
#define IDL4_PUBLISH_ATTR __attribute__ ((regparm (2))) __attribute__ ((noreturn))


struct L4VM_server_cmd;
typedef struct L4VM_server_cmd L4VM_server_cmd_t;

typedef void (*L4VM_server_cmd_handler_t)( L4VM_server_cmd_t *, void *data );

struct L4VM_server_cmd
{
    volatile L4VM_server_cmd_handler_t handler;
    L4_ThreadId_t reply_to_tid;
    void *data;
};

#define L4VM_SERVER_CMD_RING_LEN	8



typedef struct L4VM_server_cmd_ring
{
    L4_Word16_t next_free;
    L4_Word16_t next_cmd;
    volatile int wake_server;
    L4_ThreadId_t server_tid;
    L4VM_server_cmd_t cmds[L4VM_SERVER_CMD_RING_LEN];
} L4VM_server_cmd_ring_t;


extern inline L4_Word_t irq_status_reset( volatile unsigned *status )
{
    L4_Word_t events;

    do {
	events = *status;
    } while( cmpxchg(status, events, 0) != events );

    return events;
}

#define L4_TAG_IRQ      0x100

extern void L4VM_server_deliver_irq( L4_ThreadId_t irq_tid,
				     L4_Word_t irq_no,
				     L4_Word_t irq_flags,
				     volatile unsigned *irq_status,
				     unsigned irq_mask,
				     volatile unsigned *irq_pending);

extern L4_Word16_t L4VM_server_cmd_allocate(
    L4VM_server_cmd_ring_t *ring,
    L4_ThreadId_t irq_tid,
    L4_Word_t irq_no,
    L4_Word_t irq_flags,
    volatile unsigned *irq_status,
    unsigned irq_mask,
    volatile unsigned *irq_pending);

extern void L4VM_server_cmd_dispatcher( 
	L4VM_server_cmd_ring_t *cmd_ring,
	void *data,
	L4_ThreadId_t server_tid);

extern void L4VM_server_cmd_init( L4VM_server_cmd_ring_t *cmd_ring );

extern int L4VM_server_register_location( 
	guid_t guid,
	L4_ThreadId_t tid );

extern int L4VM_server_locate(
	guid_t guid,
	L4_ThreadId_t *server_tid);

#endif	/* __L4KA_DRIVERS__GLUE__VMSERVER_H__ */

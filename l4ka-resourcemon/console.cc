/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     console.c
 * Description:   Implements a virtual console device for the virtual machines.
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

#include <resourcemon.h>
#include "resourcemon_idl_server.h"
#include <debug.h>
#include <macros.h>
#include <hthread.h>
#include <vm.h>
#include <ia32/ioport.h>


char dbuf[IConsole_max_len+1];
char vmbuf[MAX_VM][IConsole_max_len+1];
L4_Word_t vmlen[MAX_VM];

IDL4_INLINE void IResourcemon_put_chars_implementation(
    CORBA_Object _caller UNUSED,
    const IConsole_handle_t handle UNUSED,
    const IConsole_stream_t *stream,
    idl4_server_environment *_env UNUSED)
{
    vm_t *vm;
    
    if( (vm = get_vm_allocator()->tid_to_vm(_caller)) == NULL)
    {
	// Don't respond to invalid clients.
	idl4_set_no_response( _env );
	printf( PREFIX "unknown client %p\n", RAW(_caller) );
	return;
    }

    word_t len = min(stream->len, IConsole_max_len);
    
    if (len)
    {
	for (word_t i=0; i < len; i++)
	    dbuf[i] = stream->raw[i] ? stream->raw[i] : ' ';

	dbuf[len] = 0;
	set_console_prefix("");
	l4kdb_printf("%s", dbuf);
	set_console_prefix(PREFIX);
	
	// Save one console line per VM
	word_t space_id = vm->get_space_id();

	for (word_t i=0; i < stream->len; i++)
	{
	    vmbuf[space_id][vmlen[space_id]] = stream->raw[i];
	    if (stream->raw[i] == '\n' || stream->raw[i] == '\r')
		vmlen[space_id] = 0;
	    else
		vmlen[space_id] = (vmlen[space_id] + 1) % IConsole_max_len;
	}
    }
}
IDL4_PUBLISH_IRESOURCEMON_PUT_CHARS(IResourcemon_put_chars_implementation);


IDL4_INLINE void IResourcemon_get_chars_implementation(
	CORBA_Object _caller UNUSED,
	const IConsole_handle_t handle UNUSED,
	IConsole_stream_t *stream,
	idl4_server_environment *_env UNUSED)
{
    stream->len = 0;
}
IDL4_PUBLISH_IRESOURCEMON_GET_CHARS(IResourcemon_get_chars_implementation);

#define COMPORT 0x3f8


void console_read()
{
    static u8_t lc = 0;
    static word_t focus_vm = 1;

    if ((in_u8(COMPORT+5) & 0x01) == 0)
	return;
    
    u8_t c = in_u8(COMPORT);
    
    if (c == 0x1b)
    { 
	if (lc == 0x1b)
	{
	    lc = 0;
	    l4kdb_printf("\n");
	    L4_KDB_Enter("kbreakin");
	}
	else
	{
	    lc = c;
	}
    }
    else if (c == 'n' && lc == 0x1b)
    {
	for (L4_Word_t sid = 0 ; sid < MAX_VM; sid++)
	{
	    focus_vm = (focus_vm+1) % MAX_VM;
	    vm_t *vm = get_vm_allocator()->space_id_to_vm(focus_vm);
	    if( vm && vm->get_client_shared())
		break;
	}
	l4kdb_printf("\n---- VM Focus %d ---\n", focus_vm); 
	
	// Restore one VM line
	for (word_t i=0; i < vmlen[focus_vm]; i++)
	    dbuf[i] = vmbuf[focus_vm][i] ? vmbuf[focus_vm][i] : ' ';

	vmbuf[focus_vm][vmlen[focus_vm]] = 0;
	l4kdb_printf("%s", vmbuf[focus_vm]);
	
	lc = 0;
    }
    else if (c == 's' && lc == 0x1b)
    {
	if (dbg_level)
	{
	    l4kdb_printf("\n---- Silent Console ---\n"); 
	    dbg_level = 0;
	}
	else
	{
	    l4kdb_printf("\n---- Verbose Console ---\n"); 	
	    dbg_level = DEFAULT_DBG_LEVEL;
	}
	lc = 0;
    }
    else
    {
	vm_t *vm = get_vm_allocator()->space_id_to_vm(focus_vm);
	if( vm && vm->get_client_shared())
	{
	    if (lc == 0x1b)
		vm->add_console_rx(lc);
	    vm->add_console_rx(c);
	}
	lc = c;
    }		 
    
    
}
void console_reader(
    void *param UNUSED,
    hthread_t *htread UNUSED)
{
    L4_Time_t sleep = L4_TimePeriod( 100 * 1000 );
    
    while (1)
    {
	console_read();
	L4_Sleep(sleep);
    }
}

void console_init()
{
#if 1
    //dbg_level = 0;
    L4_KDB_ToggleBreakin();
	
    if (!l4_pmsched_enabled)
    {
	/* Start console thread */
	hthread_t *console_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_console, 252, false, console_reader);
	    
	if( !console_thread )
	{
	    printf("couldn't start console thread");
	    L4_KDB_Enter();
	    return;
	}
	printf("Console thread TID: %t\n", console_thread->get_global_tid());
	
	console_thread->start();
    }
#endif
}

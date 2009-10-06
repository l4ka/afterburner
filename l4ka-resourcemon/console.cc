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


char buf[IConsole_max_len+1];

IDL4_INLINE void IResourcemon_put_chars_implementation(
    CORBA_Object _caller ATTR_UNUSED_PARAM,
    const IConsole_handle_t handle ATTR_UNUSED_PARAM,
    const IConsole_content_t *content,
    idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    word_t len = min(content->len, IConsole_max_len);
    
    if (len)
    {
	for (word_t i=0; i < len; i++)
	    buf[i] = content->raw[i] ? content->raw[i] : ' ';
    
	buf[len] = 0;
	set_console_prefix("");
	l4kdb_printf("%s", buf);
	set_console_prefix(PREFIX);
    }
}
IDL4_PUBLISH_IRESOURCEMON_PUT_CHARS(IResourcemon_put_chars_implementation);


IDL4_INLINE void IResourcemon_get_chars_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const IConsole_handle_t handle ATTR_UNUSED_PARAM,
	IConsole_content_t *content,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    content->len = 0;
}
IDL4_PUBLISH_IRESOURCEMON_GET_CHARS(IResourcemon_get_chars_implementation);

#define COMPORT 0x3f8

void console_read()
{

    if ((in_u8(COMPORT+5) & 0x01) == 0)
	return;
    
    u8_t c = in_u8(COMPORT);
    
    if (c == 0x1b)
    {
	L4_KDB_Enter("kbreakin");
    }
    else
    {
	for (L4_Word_t sid = 0 ; sid < MAX_VM; sid++)
	{
	    vm_t *vm = get_vm_allocator()->space_id_to_vm(sid);
	    if( vm && vm->get_client_shared())
		vm->set_console_rx(c);
	}		 
    }
    
}
void console_reader(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    L4_Time_t sleep = L4_TimePeriod( 100 * 1000 );
    L4_KDB_Enter("Console thread");
    
    while (1)
    {
	console_read();
	L4_Sleep(sleep);
    }
}

void console_init()
{
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

}

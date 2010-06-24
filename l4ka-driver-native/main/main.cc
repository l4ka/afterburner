/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     l4ka-driver-native/main/main.cc
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
 * $Id$
 *                
 ********************************************************************/

#include <l4/kdebug.h>
#include <l4/ipc.h>

#include <common/hthread.h>
#include <common/resourcemon.h>
#include <common/console.h>

static NORETURN
void master_loop()
{
    L4_ThreadId_t tid = L4_nilthread;
    for (;;)
    {
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );

	if( L4_IpcFailed(tag) ) {
	    tid = L4_nilthread;
	    continue;
	}

	printf( "Unhandled message %p from TID %p\n",
		(void *)tag.raw, (void *)tid.raw );
	L4_KDB_Enter("unhandled message");
	tid = L4_nilthread;
    }
}

void drivers_main()
{
    console_init( kdebug_putc, "\e[1m\e[33mdrivers:\e[0m " );
    printf( "L4Ka Native Drivers Project\n" );

    get_hthread_manager()->init( resourcemon_shared.thread_space_start,
	    resourcemon_shared.thread_space_len );

#if defined(cfg_net)
    extern L4_ThreadId_t net_init();
    net_init();
    extern L4_ThreadId_t lanaddress_init();
    lanaddress_init();
#endif

    if( !resourcemon_continue_init() )
	printf( "Failed to continue with system initialization.\n" );

    master_loop();
}


/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     server.cc
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

#include <l4/types.h>
#include <debug.h>
#include <resourcemon.h>
#include "resourcemon_idl_server.h"


void IResourcemon_server(void)
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long cnt;
    static void *IResourcemon_vtable_0[IRESOURCEMON_DEFAULT_VTABLE_SIZE] = IRESOURCEMON_DEFAULT_VTABLE_0;
    static void *IResourcemon_vtable_1[IRESOURCEMON_DEFAULT_VTABLE_SIZE] = IRESOURCEMON_DEFAULT_VTABLE_1;
    static void *IResourcemon_vtable_2[IRESOURCEMON_DEFAULT_VTABLE_SIZE] = IRESOURCEMON_DEFAULT_VTABLE_2;
    static void *IResourcemon_vtable_discard[IRESOURCEMON_DEFAULT_VTABLE_SIZE] = IRESOURCEMON_DEFAULT_VTABLE_DISCARD;
    static void *IResourcemon_ktable[IRESOURCEMON_DEFAULT_KTABLE_SIZE] = IRESOURCEMON_DEFAULT_KTABLE;
    static void **IResourcemon_itable[4] = { IResourcemon_vtable_0, IResourcemon_vtable_1, IResourcemon_vtable_2, IResourcemon_vtable_discard };

    idl4_msgbuf_init(&msgbuf);

    while (1)
    {
       	partner = L4_nilthread;
	msgtag.raw = 0;
	cnt = 0;

	while (1)
	{
	    idl4_msgbuf_sync(&msgbuf);

	    idl4_reply_and_wait(&partner, &msgtag, &msgbuf, &cnt);

	    if (idl4_is_error(&msgtag))
		break;

	    if (IDL4_EXPECT_FALSE(idl4_is_kernel_message(msgtag)))
		idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IResourcemon_ktable[idl4_get_kernel_message_id(msgtag) & IRESOURCEMON_KID_MASK]);
	    else idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IResourcemon_itable[idl4_get_interface_id(&msgtag) & IRESOURCEMON_IID_MASK][idl4_get_function_id(&msgtag) & IRESOURCEMON_FID_MASK]);
	}
    }
}



void IResourcemon_discard(void)
{
    L4_KDB_Enter("Resourcemon discard");
  printf(PREFIX "discard request\n");
}


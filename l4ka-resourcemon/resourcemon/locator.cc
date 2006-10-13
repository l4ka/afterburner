/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/locator.cc
 * Description:   Implements a service finder.
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

#include <l4/kip.h>
#include <l4/thread.h>

#include <common/debug.h>
#include <resourcemon/resourcemon.h>
#include "resourcemon_idl_client.h"
#include "resourcemon_idl_server.h"

#define MAX_UUID 10
static L4_ThreadId_t locator_services[MAX_UUID];


IDL4_INLINE void IResourcemon_query_interface_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM, 
	const guid_t guid, 
	L4_ThreadId_t *tid, 
	idl4_server_environment *_env)
{
    if( (guid < MAX_UUID) && (!L4_IsNilThread(locator_services[guid])) )
	*tid = locator_services[guid];
    else
	CORBA_exception_set( _env, ex_ILocator_unknown_interface, NULL );
}
IDL4_PUBLISH_IRESOURCEMON_QUERY_INTERFACE(IResourcemon_query_interface_implementation);


IDL4_INLINE void IResourcemon_register_interface_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const guid_t guid,
	const L4_ThreadId_t *tid,
	idl4_server_environment *_env)
{
    if( guid < MAX_UUID )
	locator_services[guid] = *tid;
    else
	CORBA_exception_set( _env, ex_ILocator_invalid_guid_format, NULL );
}
IDL4_PUBLISH_IRESOURCEMON_REGISTER_INTERFACE(IResourcemon_register_interface_implementation);

void register_interface( guid_t guid, L4_ThreadId_t tid )
{
    void *kip = L4_GetKernelInterface();
    L4_ThreadId_t locator_tid = L4_GlobalId( 2+L4_ThreadIdUserBase(kip), 1 );
    ASSERT( locator_tid != L4_Myself() );


    CORBA_Environment ipc_env = { 
	_major: 0, _minor: 0, _data: NULL, _timeout: L4_Timeouts(L4_Never, L4_Never), _rcv_window: { raw : 0} };
    //idl4_default_environment;
    IResourcemon_register_interface( locator_tid, guid, &tid, &ipc_env );
    ASSERT( ipc_env._major != CORBA_USER_EXCEPTION );
}


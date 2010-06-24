/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     l4ka-driver-native/common/resourcemon.cc
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

#include <common/resourcemon.h>
#include <common/console.h>

bool resourcemon_register_service( guid_t guid, L4_ThreadId_t tid )
{
    CORBA_Environment ipc_env = idl4_default_environment;

    IResourcemon_register_interface( get_locator_tid(), guid, &tid, &ipc_env );

    return ipc_env._major == CORBA_NO_EXCEPTION;
}

bool resourcemon_tid_to_space_id( L4_ThreadId_t tid, word_t *space_id )
{
    ASSERT( space_id );

    CORBA_Environment ipc_env = idl4_default_environment;
    IResourcemon_tid_to_space_id( get_resourcemon_tid(), &tid, 
	    (L4_Word_t *)space_id, &ipc_env );

    return ipc_env._major == CORBA_NO_EXCEPTION;
}

bool resourcemon_get_space_info( L4_ThreadId_t tid, space_info_t *info )
{
    ASSERT( info );

    if( !resourcemon_tid_to_space_id(tid, &info->space_id) )
	return false;

    CORBA_Environment ipc_env = idl4_default_environment;
    IResourcemon_get_space_phys_range( get_resourcemon_tid(),
	    info->space_id, 
	    (L4_Word_t *)&info->bus_start, (L4_Word_t *)&info->bus_size, 
	    &ipc_env );

    return ipc_env._major == CORBA_NO_EXCEPTION;
}

bool resourcemon_continue_init()
{
    CORBA_Environment ipc_env = idl4_default_environment;
    IResourcemon_client_init_complete( get_resourcemon_tid(), &ipc_env );
    return ipc_env._major == CORBA_NO_EXCEPTION;
}


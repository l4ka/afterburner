/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/src/l4app/Resourcemon.cpp
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
/*
 * wrapper for resourcemon access functions
 */
#include <l4/kdebug.h>
#include <l4/types.h>
#include "l4app/Resourcemon.h"
#include "resourcemon_idl_client.h"
#include <iostream>

namespace l4app
{

    L4_Word_t vminfo_page[1024];
/**
 * @brief get a VMInfo structure from the RM
 */
int
Resourcemon::getVMInfo(L4_Word_t spaceId, VMInfo& vmInfo) 
{
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Word_t result;
    idl4_fpage_t fp;
    L4_Fpage_t vminfo_fp;
    vminfo_fp = L4_FpageLog2( (L4_Word_t) vminfo_page, 12);
    idl4_set_rcv_window( &ipc_env, vminfo_fp);
    
    IResourcemon_vm_info(getResourcemonTid(),
			 spaceId,
			 &fp,
			 &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	result = CORBA_exception_id(&ipc_env) - ex_IResourcemon_ErrOk;
	CORBA_exception_free( &ipc_env );
	std::cerr << "IResourcemon_vm_info: " << result << std::endl;
	return -1;
    }
    else
	return 0;
}

/**
 * @brief map VM space into this address space
 */
int
Resourcemon::getVMSpace(L4_Word_t spaceId, char*& vm_space) 
{
    CORBA_Environment ipc_env = idl4_default_environment;
    idl4_fpage_t fp = { base : 0 };
    L4_Word_t result;
    IResourcemon_get_vm_space(getResourcemonTid(),
	    spaceId,
	    &fp,
	    &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	result = CORBA_exception_id(&ipc_env) - ex_IResourcemon_ErrOk;
	CORBA_exception_free( &ipc_env );
	std::cerr << "IResourcemon_get_vm_space: " << result << std::endl;
	return -1;
    }
    L4_Fpage_t l4fp = { raw : fp.fpage };
    vm_space = (char *) L4_Address(l4fp); // XXX check this
    return 0;
}

/**
 * @brief delete the local VM after successful migration
 */
int
Resourcemon::deleteVM(L4_Word_t spaceId) 
{
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Word_t result;
	IResourcemon_delete_vm(getResourcemonTid(),
		spaceId,
		&ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
		result = CORBA_exception_id(&ipc_env) - ex_IResourcemon_ErrOk;
		CORBA_exception_free( &ipc_env );
		std::cerr << "IResourcemon_delete_vm: " << result << std::endl;
		return -1;
    }
	else
		return 0;
}

/**
 * @brief reserve a new VM container based on VMInfo
 */
int
Resourcemon::reserveVM(VMInfo& vmInfo, L4_Word_t *spaceId)
{
#if 0
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Word_t result;
	IResourcemon_reserve_vm(getResourcemonTid(),
		vmInfo,
		spaceId,
		&ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
		result = CORBA_exception_id(&ipc_env) - ex_IResourcemon_ErrOk;
		CORBA_exception_free( &ipc_env );
		std::cerr << "IResourcemon_reserve_vm: " << result << std::endl;
		return -1;
    }
	else
		return 0;
#endif
    L4_KDB_Enter("UNIMPLEMENTED reserveVM");
    return 0;
}
/**
 * @brief map migrated VM space to local RM
 */
int
Resourcemon::mapVMSpace(L4_Word_t spaceId, const char *vmSpace) 
{
#if 0
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Word_t result;
	IResourcemon_map_vm_space(getResourcemonTid(),
		spaceId,
		vmSpace,
		&ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
		result = CORBA_exception_id(&ipc_env) - ex_IResourcemon_ErrOk;
		CORBA_exception_free( &ipc_env );
		std::cerr << "IResourcemon_map_vm_space: " << result << std::endl;
		return -1;
    }
	else
		return 0;
#endif
    L4_KDB_Enter("UNIMPLEMENTED mapVMSpace");
    return 0;
}

}; // namespace l4app

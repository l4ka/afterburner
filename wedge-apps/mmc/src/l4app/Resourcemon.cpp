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

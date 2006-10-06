
#include <common/resourcemon.h>
#include <common/console.h>

bool resourcemon_register_service( guid_t guid, L4_ThreadId_t tid )
{
    CORBA_Environment ipc_env = idl4_default_environment;

    IHypervisor_register_interface( get_locator_tid(), guid, &tid, &ipc_env );

    return ipc_env._major == CORBA_NO_EXCEPTION;
}

bool resourcemon_tid_to_space_id( L4_ThreadId_t tid, word_t *space_id )
{
    ASSERT( space_id );

    CORBA_Environment ipc_env = idl4_default_environment;
    IHypervisor_tid_to_space_id( get_resourcemon_tid(), &tid, 
	    (L4_Word_t *)space_id, &ipc_env );

    return ipc_env._major == CORBA_NO_EXCEPTION;
}

bool resourcemon_get_space_info( L4_ThreadId_t tid, space_info_t *info )
{
    ASSERT( info );

    if( !resourcemon_tid_to_space_id(tid, &info->space_id) )
	return false;

    CORBA_Environment ipc_env = idl4_default_environment;
    IHypervisor_get_space_phys_range( get_resourcemon_tid(),
	    info->space_id, 
	    (L4_Word_t *)&info->bus_start, (L4_Word_t *)&info->bus_size, 
	    &ipc_env );

    return ipc_env._major == CORBA_NO_EXCEPTION;
}

bool resourcemon_continue_init()
{
    CORBA_Environment ipc_env = idl4_default_environment;
    IHypervisor_client_init_complete( get_resourcemon_tid(), &ipc_env );
    return ipc_env._major == CORBA_NO_EXCEPTION;
}


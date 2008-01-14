#include <l4/thread.h>
#include <l4/message.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <l4/kdebug.h>
#include <l4/kip.h>

#include "common/string.h"
#include "resourcemon_idl_server.h"
#include "resourcemon/vm.h"
#include "resourcemon/migration.h"

#define L4_TAG_MIGRATION 0xffa

// used by the migration subsystem of the resourcemonitor
// to manage all migration relevant information for each VM
#if 0
class migration_data_t
{
public:
    L4_ThreadId_t monitor_tid;
};
#endif
//static migration_data_t migration_vms[MAX_VM];

// this type is used to send migration information
// to the destination host
class vm_migration_data_t
{
public:
    vm_migration_data_t(vm_t *source_vm) : vm(source_vm) {}
    ~vm_migration_data_t() {}
    L4_Word_t vm_size;
    L4_Word_t vaddr_offset;  // virtual AS offset (2GB)
    L4_Word_t wedge_size;
    L4_Word_t wedge_paddr;
    //my_thread_info_t ti;
    vm_t *vm;
    
private:
};

static inline void copy_vm(vm_t *old_vm, vm_t *new_vm)
{
    printf( "copying VM from %x to %x\n", 
	    old_vm->get_haddr_base(), new_vm->get_haddr_base());
    memcpy((void*)new_vm->get_haddr_base(),
	   (void*)old_vm->get_haddr_base(),
	   old_vm->get_space_size());
}

static vm_t *allocate_vm_from_info(VMInfo *vmInfo)
{
    vm_t *new_vm;
    if ((new_vm = get_vm_allocator()->allocate_vm()) == NULL)
    {
	printf( "unable to allocate a new VM\n");
	return NULL;
    }
    printf( "new VM allocatd\n");
    if (!new_vm->init_mm(vmInfo->space_size,
			 vmInfo->vaddr_offset,
			 true,
			 vmInfo->wedge_size,
			 vmInfo->wedge_paddr))
    {
	printf( "unable to allocate new VM memory\n");
    	get_vm_allocator()->deallocate_vm(new_vm);
	return NULL;
    }
    printf( "new VM memory allocated\n");
    return new_vm;
}

//
// allocate new vm_t object and init the VM's memory
//
static vm_t *allocate_vm(vm_t *old_vm)
{
	// allocate new VM object
	vm_t *new_vm;
	if ((new_vm = get_vm_allocator()->allocate_vm()) == NULL)
	{
		printf( "unable to allocate a new VM\n");
		return NULL;
	}
	printf( "new VM allocated\n");
	
	// init memory for new VM
	if (!new_vm->init_mm(old_vm->get_space_size(),	// vmsize
		old_vm->get_vaddr_offset(),					// 2GB VM offset
		true,										// shadow_special
		old_vm->get_wedge_size(),					// wedgesize
		old_vm->get_wedge_paddr()))					// wedgeinstall
		{
			printf( "unable to allocate new VM memory\n");
    		get_vm_allocator()->deallocate_vm(new_vm);
			return NULL;
		}
	printf( "new VM memory allocated\n");
	return new_vm;
}

//
// VM clone implementation, to be called from within resourcemonitor
//
static vm_t *do_clone_vm(L4_Word_t source_id)
{
    // determine source VM
    vm_t *source_vm = get_vm_allocator()->space_id_to_vm(source_id);
    //vm_migration_data_t vm_migration_data(source_vm);
    if (source_vm == NULL)
    {
	printf( "Error: source_vm is NULL\n");
	goto err_out;
    }
    
    // create new VM
    vm_t *new_vm = allocate_vm(source_vm);
    if (new_vm == NULL)
    {
	printf( "Error: could not allocate new VM\n");
	goto err_out;
    }
    
    // copy memory from old VM to new VM
    copy_vm(source_vm, new_vm);
    printf( "copied VM memory\n");

    // copy the client shared memory region
    // from the source vm_t object to the new vm_t object
    new_vm->copy_client_shared(source_vm);
    printf( "VM <-> RM shared memory region copied\n");

    // init the ramdisk module
    new_vm->set_ramdisk_start(source_vm->get_ramdisk_start());
    new_vm->set_ramdisk_size(source_vm->get_ramdisk_size());
    printf( "installed ramdisk at VM address %x size %d\n",
	    new_vm->get_ramdisk_start(), new_vm->get_ramdisk_size());

    new_vm->set_binary_start_vaddr(source_vm->get_binary_start_vaddr());
    new_vm->set_binary_end_vaddr(source_vm->get_binary_end_vaddr());

    // setup the KIP and UTCB, use values from source_vm
    if (!new_vm->install_memory_regions(source_vm))
    {
	printf( "could not install memory regions\n");
	goto err_out;
    }

    // init the shared memory region
    new_vm->init_client_shared(source_vm->get_cmdline());
    printf( "VM <-> RM shared memory region initialized\n");

    new_vm->set_entry_ip(source_vm->get_entry_ip());

    source_vm->dump_vm();

    return new_vm;
err_out:
    return NULL;
}

//
// VM clone entry point, called from a userspace application
//
//IDL4_INLINE void IResourcemon_clone_vm_implementation(
//    CORBA_Object _caller ATTR_UNUSED_PARAM,
//    L4_ThreadId_t source_tid,
//    L4_Word_t start_func,
//    L4_Word_t *dest_id,
//    idl4_server_environment *_env)
IDL4_INLINE void IResourcemon_clone_vm_implementation(
    CORBA_Object _caller ATTR_UNUSED_PARAM,
    const L4_ThreadId_t *tid,
    L4_Word_t start_func,
    idl4_server_environment *_env)
{
    L4_Word_t source_id = tid_space_t::tid_to_space_id(*tid);
    //L4_Word_t start_func = 0;
    vm_t *new_vm;
    printf( "clone_vm request\n");
    if ((new_vm = do_clone_vm(source_id)) == NULL)
    {
	printf( "clone_vm failed\n");
	goto err_out;
    }

    // what we have so far:
    // 1. initialized VM data structure
    // 2. all VM memory copied
    // 3. RM <-> VM shared memory region

    // what to do:
    // 1. restart the VM monitor thread
    // 2. send resume VM to the monitor thread

    /**
     * start_vm:
     *  - set the main thread's IP to the migration_resume function
     *  - set the VM's main thread ID to first_tid, first_tid is determined
     *    dynamically using the VM's space_id
     *  - init the main thread's AS
     *  - set the main thread's priority to this->get_prio()
     *  - set the main thread's timeslice to never
     *  - start the main thread
     *  - activate the main thread
     */
    printf( "setting start to vaddr ", (void*)start_func, "\n");
    new_vm->set_binary_entry_vaddr(start_func);
    if (!new_vm->start_vm())
    {
	printf( "error starting cloned VM\n");
	goto err_out;
    }
    printf( "cloned VM restarted\n");
    new_vm->dump_vm();

    // set the ID of the clone in the parent AS
    L4_Word_t dest_id = new_vm->get_space_id();
    dest_id = dest_id;

    return;

 err_out:
    idl4_set_no_response(_env);
    return;
}
IDL4_PUBLISH_IRESOURCEMON_CLONE_VM(IResourcemon_clone_vm_implementation);

//
// send migration initialization request to the VM's monitor thread
// when the VM monitor thread replys then all other VM threads,
// which run at a lower priority, are guaranteed to be halted in
// a safe state
//
#if 1
static void signal_migration(L4_Word_t monitor_tid)
{
    L4_Msg_t msg;
    L4_Clear(&msg);
    L4_MsgTag_t tag;
    tag.raw = L4_TAG_MIGRATION;
    tag.raw <<= 20;
    L4_Set_MsgTag(&msg, tag);
    L4_Load(&msg);
    L4_ThreadId_t tid;
    tid.global.X.thread_no = monitor_tid;
    L4_Send(tid);
    // XXX: check error
}
#endif

/**
 * migration protocol:
 * - LAPP requests the VMInfo from RM
 * - LAPP sends VMInfo to DAPP
 * - DAPP allocates necessary VM resources in RM
 * - DAPP sends OK to LAPP
 * - LAPP requests VM memory from RM
 * - RM signals migration to VMM
 * - VMM replys to RM thereby suspsends itself
 * - RM maps VM memory into LAPP
 * - LAPP copies VM memory to DAPP
 * - DAPP maps VM memory to RM
 * - RM copies VM memory into allocated VM memory
 * - RM restarts the VMM thread with IP=resume_vm
 * - VMM thread restarts all VM threads
 * - DAPP sends OK to LAPP and LAPP kills local VM
 */

/********************************************************************
 * LAPP requests the VMInfo from RM
 ********************************************************************/
IDL4_INLINE void IResourcemon_vm_info_implementation(CORBA_Object  _caller, 
						     const L4_Word_t  space_id, // get info for this VM space ID
						     idl4_fpage_t * vmInfoPg, // return info in this object as page
						     idl4_server_environment * _env)
{
    VMInfo dummy;
    VMInfo *vmInfo = &dummy;
	
    static const char *req_type = "vm_info request";
    printf( req_type, " from ", _caller, "\n");
    vm_t *caller_vm = get_vm_allocator()->tid_to_vm(_caller);
    if (!caller_vm)
    {
	printf( req_type, " from invalid client VM\n");
	//CORBA_exception_set(_env, ex_IResourcemon_unknown_client, NULL);
	idl4_set_no_response(_env);
	return;
    }
    // get VM information
    vm_t *migration_vm = get_vm_allocator()->space_id_to_vm(space_id);
    if (!migration_vm)
    {
	printf( req_type, " for invalid VM ID ", space_id, "\n");
	//CORBA_exception_set(_env, ex_IResourcemon_unknown_client, NULL);
	idl4_set_no_response(_env);
	return;
    }
    // fill VMInfo
    vmInfo->space_size = migration_vm->get_space_size();
    vmInfo->haddr_base = migration_vm->get_haddr_base();
    vmInfo->vaddr_offset = migration_vm->get_vaddr_offset();
    vmInfo->wedge_paddr = migration_vm->get_wedge_paddr();
    vmInfo->wedge_vaddr = migration_vm->get_wedge_vaddr();
    vmInfo->wedge_size = migration_vm->get_wedge_size();
    memcpy((void*)vmInfo->client_shared_vaddr, migration_vm->get_client_shared(),
	   vmInfo->client_shared_size);
    vmInfo->client_shared_vaddr = migration_vm->get_client_shared_vaddr();
    vmInfo->kip_start = L4_Address(migration_vm->get_kip_fp());
    vmInfo->kip_size = L4_Size(migration_vm->get_kip_fp());
    vmInfo->utcb_start = L4_Address(migration_vm->get_utcb_fp());
    vmInfo->utcb_size = L4_Size(migration_vm->get_utcb_fp());
    vmInfo->binary_stack_vaddr = migration_vm->get_binary_stack_vaddr();
    vmInfo->binary_entry_vaddr = migration_vm->get_binary_entry_vaddr();

    // energy accounting info
}
IDL4_PUBLISH_IRESOURCEMON_VM_INFO(IResourcemon_vm_info_implementation);

/********************************************************************
 * - DAPP allocates necessary VM resources in RM
 ********************************************************************/
IDL4_INLINE void IResourcemon_allocate_vm_implementation(
	CORBA_Object _caller,
	//VMInfo *vmInfo,
	idl4_server_environment *_env)
{
    VMInfo dummy;
    VMInfo *vmInfo = &dummy;
    static const char *req_type = "allocate_vm request";
    printf( req_type, " from ", _caller, "\n");
    vm_t *caller_vm = get_vm_allocator()->tid_to_vm(_caller);
    if (!caller_vm)
    {
	printf( req_type, " from invalid client VM\n");
	idl4_set_no_response(_env);
	return;
    }
    // allocate new VM based on VM information
    vm_t *new_vm;
    if ((new_vm = allocate_vm_from_info(vmInfo)) == NULL)
    {
	printf( req_type, " could not allocate new VM\n");
	return;
    }
    // TODO: return SUCCESS to caller
}
IDL4_PUBLISH_IRESOURCEMON_ALLOCATE_VM(IResourcemon_allocate_vm_implementation);

IDL4_INLINE void IResourcemon_delete_vm_implementation(CORBA_Object  _caller, const L4_Word_t  space_id, idl4_server_environment * _env)
{
    
    printf( " IResourcemon_delete_vm_implementation UNIMPLEMENTED\n");
    L4_KDB_Enter("UNIMPLEMENTED");
    
}
IDL4_PUBLISH_IRESOURCEMON_DELETE_VM(IResourcemon_delete_vm_implementation);

/********************************************************************
 * - LAPP requests VM memory from RM
 * - RM signals migration to VMM
 * - VMM replys to RM thereby suspsends itself
 * - RM maps VM memory into LAPP
 ********************************************************************/
IDL4_INLINE void IResourcemon_get_vm_space_implementation(
    CORBA_Object _caller,
    const L4_Word_t space_id,
    idl4_fpage_t *fp,
    idl4_server_environment *_env)
{
    static const char *req_type = "get_vm_space request";
    printf( req_type, " from ", _caller, "\n");
    vm_t *caller_vm = get_vm_allocator()->tid_to_vm(_caller);
    if (!caller_vm)
    {
	printf( req_type, " from invalid client VM\n");
	idl4_set_no_response(_env);
	return;
    }
    // signal migration to VMM
    vm_t *migration_vm = get_vm_allocator()->space_id_to_vm(space_id);
    if (!migration_vm)
    {
	printf( req_type, " for invalid VM ID ", space_id, "\n");
	idl4_set_no_response(_env);
	return;
    }
    L4_Word_t monitor_tid = tid_space_t::get_tid_start(space_id);
    signal_migration(monitor_tid);
    // TODO: must have a valid reply

    // map VM space into caller address space	    
    idl4_fpage_set_page(fp, L4_Nilpage);
    idl4_fpage_set_base(fp, 0);
    L4_Fpage_t migration_vm_fp = L4_Fpage(migration_vm->get_haddr_base(),
			       migration_vm->get_space_size());
    if (L4_Size(migration_vm_fp) > caller_vm->get_space_size())
    {
		printf( req_type, " not enough space in caller AS\n");
		idl4_set_no_response(_env);
		return;
    }
    idl4_fpage_set_page(fp, migration_vm_fp);
    idl4_fpage_set_base(fp, L4_Address(migration_vm_fp));
    idl4_fpage_set_mode(fp, IDL4_MODE_MAP);
    idl4_fpage_set_permissions(fp, IDL4_PERM_READ);
    
}
IDL4_PUBLISH_IRESOURCEMON_GET_VM_SPACE(IResourcemon_get_vm_space_implementation);

/********************************************************************
 * - DAPP maps VM memory to RM
 * - RM copies VM memory into allocated VM memory
 ********************************************************************/
IDL4_INLINE void IResourcemon_set_vm_space_implementation(
    CORBA_Object _caller,
    const L4_Word_t space_id,  // set space for this VM
    idl4_fpage_t *fp,          // VM memory mapping
    idl4_server_environment *_env)
{
	static const char *req_type = "set_vm_space request";
    printf( req_type, " from ", _caller, "\n");
    vm_t *caller_vm = get_vm_allocator()->tid_to_vm(_caller);
    if (!caller_vm)
    {
		printf( req_type, " from invalid client VM\n");
		idl4_set_no_response(_env);
		return;
    }
    // TODO: map VM memory into RM space
    
	// copy migrated VM memory into new VM container
    vm_t *new_vm = get_vm_allocator()->space_id_to_vm(space_id);
	if (!new_vm)
	{
		printf( req_type, " for invalid VM ID ", space_id, "\n");
		idl4_set_no_response(_env);
		return;
	}
	// TODO: save old_vm in the allocate_vm interface
	//       assign memory mapping to old_vm->haddr_base
	//       move the memory setup from do_clone_vm to copy_vm
	// need the mapping from DAPP
	//copy_vm(old_vm, new_vm);
}
IDL4_PUBLISH_IRESOURCEMON_SET_VM_SPACE(IResourcemon_set_vm_space_implementation);

void resume_vm(void)
{
}

/********************************************************************
 * - RM restarts the VMM thread with IP=resume_vm
 * - VMM thread restarts all VM threads (implicit in resume_vm)
 ********************************************************************/
IDL4_INLINE void IResourcemon_resume_vm_implementation(
    CORBA_Object _caller,
    const L4_Word_t space_id,  // set space for this VM
    idl4_server_environment *_env)
{
	static const char *req_type = "resume_vm request";
    printf( req_type, " from ", _caller, "\n");
    vm_t *caller_vm = get_vm_allocator()->tid_to_vm(_caller);
    if (!caller_vm)
    {
		printf( req_type, " from invalid client VM\n");
		idl4_set_no_response(_env);
		return;
    }
	// RM restarts the VMM thread with IP=resume_vm
	// TODO: save resume_vm address somewhere
    printf( "setting start to vaddr ", (void*)resume_vm, "\n");
    vm_t *new_vm = get_vm_allocator()->space_id_to_vm(space_id);
	if (!new_vm)
	{
		printf( req_type, " for invalid VM ID ", space_id, "\n");
		idl4_set_no_response(_env);
		return;
	}
    new_vm->set_binary_entry_vaddr((L4_Word_t)resume_vm);
	/**
     * start_vm:
     *  - set the VM's main thread ID to first_tid, first_tid is determined
     *    dynamically using the VM's space_id
     *  - init the main thread's AS
     *  - set the main thread's priority to this->get_prio()
     *  - set the main thread's timeslice to never
     *  - start the main thread
     *  - activate the main thread
     */
    if (!new_vm->start_vm())
    {
		printf( req_type, " could not start migrated VM\n");
		idl4_set_no_response(_env);
		return;
    }
    printf( "migrated VM resumed execution\n");
    new_vm->dump_vm();
}
IDL4_PUBLISH_IRESOURCEMON_RESUME_VM(IResourcemon_resume_vm_implementation);

// prototype
#if 0
IDL4_INLINE void IResourcemon_migration_implementation(
    CORBA_Object _caller,
    const L4_Word_t space_id,
    L4_Word_t *result,
    idl4_server_environment *_env)
{
    printf( "migration request from ", _caller, "\n");

    vm_t *caller_vm = get_vm_allocator()->tid_to_vm(_caller);
    if (!caller_vm)
    {
	printf( "migration request from invalid client VM\n");
	idl4_set_no_response(_env);
	return;
    }

    vm_t *migration_vm = get_vm_allocator()->space_id_to_vm(space_id);
    if (!migration_vm)
    {
	printf( "migration request for invalid VM ID ", space_id, "\n");
	idl4_set_no_response(_env);
	return;
    }

    // signal migration request to VM monitor
    // a thread within the VM monitor is waiting for a migration request
    // from the resourcemonitor
    signal_migration(migration_vm->get_space_id());
}
IDL4_PUBLISH_IRESOURCEMON_MIGRATION(IResourcemon_migration_implementation);
#endif

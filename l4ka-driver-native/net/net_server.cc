/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     l4ka-driver-native/net/net_server.cc
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

#include <l4/schedule.h>
#include <l4/kip.h>

#include <common/ia32/page.h>
#include <common/resourcemon.h>
#include <common/l4privileged.h>
#include <common/hthread.h>
#include <common/valloc.h>

#include <pci/pci_config.h>
#include <net/net.h>
#include <net/e1000/e1000_driver.h>
#include <net/lanaddress_server.h>

#include "L4VMnet_idl_server.h"


/****************************************************************************
 *
 *  PCI device scanner
 *
 ****************************************************************************/


class E1000Scanner : public PciScanner
{
public:
    static const int max_devices = 1;

protected:
    int tot_devices;
    E1000Driver drivers[max_devices];

public:
    // Query functions.
    int get_tot_devices() { return this->tot_devices; }

    E1000Driver *get_driver( int which )
    {
	if( which > this->get_tot_devices() )
	    return NULL;
	return &this->drivers[which];
    }

    // Setup functions.
    E1000Scanner() { this->tot_devices = 0; }
    void assign_interrupt_cpus( int cpu_cnt );
    void associate_thread_interrupts();
    bool open_all_devices();

    // Action functions.
    inline void handle_interrupt()
    {
	for( int i = 0; i < this->tot_devices; i++ )
	    this->drivers[i].irq_handler( L4_nilthread );
    }

protected:
    virtual bool inspect_device( PciDeviceConfig *pci_config )
    {
	u16_t vendor_id = pci_config->get_vendor_id();
	u16_t device_id = pci_config->get_device_id();

	if( (vendor_id != 0x8086) || (device_id != 0x100e) )
	    return true;	// Continue searching.

	if( this->tot_devices >= this->max_devices ) {
	    printf( "e1000: Found too many Intel 1000 devices.\n" );
	    L4_KDB_Enter( "too many e1000 devices" );
	    return false;	// Stop searching.
	}

	E1000Driver *driver = &this->drivers[ this->tot_devices ];
	if( !driver->init(this->tot_devices, pci_config) )
	    return false;	// Stop searching, because we polluted the
				// driver and haven't written code to clean it.

	this->tot_devices++;
	return true;	// Continue searching.
    }
};

void E1000Scanner::assign_interrupt_cpus( int cpu_cnt )
{
    bool shared;
    L4_ThreadId_t tid;

    int cpu = 0;
    for( int which = 0; which < this->tot_devices; which++ ) {
	tid = this->drivers[which].get_irq_tid();

	// Did we already obtain this interrupt?
	shared = false;
	for( int search = 0; !shared && (search < which); search++ )
	    shared = (this->drivers[search].get_irq_tid() == tid);
	if( shared ) {
	    nprintf( 1, "e1000: shared irq: %ld\n", L4_ThreadNo(tid) );
	    continue;
	}

	nprintf( 1, "lxserv::net: assigning cpu %d to irq %ld\n",
		cpu, L4_ThreadNo(tid) );

	this->drivers[which].cpu = cpu;
	cpu = (cpu + 1) % cpu_cnt;
    }
}

void E1000Scanner::associate_thread_interrupts()
    // This function must be executed from an event thread, on the cpu
    // for which the interrupts should be attached.
{
    L4_ThreadId_t tid;

    for( int which = 0; which < this->tot_devices; which++ )
    {
	// Does the interrupt belong to our cpu?
	if( this->drivers[which].cpu != L4_ProcessorNo() )
	    continue;
	tid = this->drivers[which].get_irq_tid();

	nprintf( 2, "lxserv::net: associating irq %ld with thread %lx on "
		"cpu %d\n",
		L4_ThreadNo(tid), L4_Myself().raw, L4_ProcessorNo() );

	// Associate with the interrupt.
	if( L4_ErrOk != AssociateInterrupt(tid, L4_Myself(), get_max_prio(), L4_ProcessorNo()))
	    nprintf( 1, "lxserv::net: error, unable to associate with "
		    "interrupt %ld.\n", L4_ThreadNo(tid) );

    }
}

bool E1000Scanner::open_all_devices()
{
    for( int which = 0; which < this->tot_devices; which++ )
	if( !this->drivers[which].open() )
	    return false;
    return true;
}

/****************************************************************************
 *
 *  Global variables
 *
 ****************************************************************************/

static E1000Scanner manager;
NetIfaceMap net_iface_map;
static L4_ThreadId_t net_cpu_map[cfg_max_cpu];

NetServerShared device_server_status[ E1000Scanner::max_devices ] ALIGNED(PAGE_SIZE) SECTION(".aligned");
NetClientShared client_shared_regions[cfg_max_net_clients] ALIGNED(sizeof(NetClientShared)) SECTION(".aligned");

static const word_t stack_size = KB(8);
static u8_t thread_stacks[stack_size * cfg_max_cpu];

INLINE word_t get_stack_top( word_t cpu )
{
    return (word_t) &thread_stacks[ (stack_size+1) * cpu ];
}

INLINE word_t get_stack_bottom( word_t cpu )
{
    return (word_t) &thread_stacks[ stack_size * cpu ];
}

/****************************************************************************
 *
 *  Server Interfaces
 *
 ****************************************************************************/

IDL4_INLINE void IVMnet_Control_interrupt_implementation(
	CORBA_Object _caller,
	idl4_server_environment *_env)
{
    manager.handle_interrupt();
}
IDL4_PUBLISH_IVMNET_CONTROL_INTERRUPT(IVMnet_Control_interrupt_implementation);
IDL4_PUBLISH_IVMNET_EVENTS_INTERRUPT(IVMnet_Control_interrupt_implementation);


IDL4_INLINE void IVMnet_Control_attach_implementation(
        CORBA_Object _caller,
        const char *dev_name,
        const char *lan_address,
        IVMnet_handle_t *handle,
        idl4_fpage_t *shared_window,
        idl4_fpage_t *server_status,
        idl4_server_environment *_env)
{   
    *handle = lanaddress_get_handle( (lanaddress_t *)lan_address );

    printf( "Attach request from handle %u, lan address %0.6b, for device %s\n",
	    *handle, lan_address, dev_name );

    NetIface *iface = get_iface_map()->get_iface( *handle );
    if( !iface ) {
	CORBA_exception_set( _env, ex_IVMnet_invalid_handle, NULL);
	return;
    }

    E1000Driver *driver = manager.get_driver(0);
    if( !driver ) {
	CORBA_exception_set( _env, ex_IVMnet_invalid_device, NULL );
	return;
    }

    iface->init( *handle, driver, get_client_shared(*handle) );
    iface->set_lan_addr( (const u8_t *)lan_address );

    if( !resourcemon_get_space_info(_caller, &iface->client_space) ) {
	CORBA_exception_set( _env, ex_IVMnet_invalid_client, NULL);
	return;
    }

    NetClientShared *client_shared = iface->get_client_shared();
    NetServerShared *server_shared = get_server_shared( iface->get_driver()->get_index() );
    client_shared->clear();

    // Reply to the client
    idl4_fpage_set_base( shared_window, 0 );
    idl4_fpage_set_mode( shared_window, IDL4_MODE_MAP );
    idl4_fpage_set_page( shared_window, L4_Fpage((word_t)client_shared, sizeof(NetClientShared)) );
    idl4_fpage_set_permissions( shared_window, IDL4_PERM_WRITE | IDL4_PERM_READ );

    idl4_fpage_set_base( server_status, sizeof(NetClientShared) );
    idl4_fpage_set_mode( server_status, IDL4_MODE_MAP );
    idl4_fpage_set_page( server_status, L4_Fpage((word_t)server_shared, sizeof(NetServerShared)) );
    idl4_fpage_set_permissions( server_status, IDL4_PERM_WRITE | IDL4_PERM_READ );
}
IDL4_PUBLISH_IVMNET_CONTROL_ATTACH(IVMnet_Control_attach_implementation);


IDL4_INLINE void IVMnet_Control_reattach_implementation(
        CORBA_Object _caller,
        const IVMnet_handle_t handle,
        idl4_fpage_t *shared_window,
        idl4_fpage_t *server_status,
        idl4_server_environment *_env)
{
    UNIMPLEMENTED();
}
IDL4_PUBLISH_IVMNET_CONTROL_REATTACH(IVMnet_Control_reattach_implementation);


IDL4_INLINE void IVMnet_Control_detach_implementation(
        CORBA_Object _caller,
        const IVMnet_handle_t handle,
        idl4_server_environment *_env)
{
    UNIMPLEMENTED();
}
IDL4_PUBLISH_IVMNET_CONTROL_DETACH(IVMnet_Control_detach_implementation);


IDL4_INLINE void IVMnet_Control_start_implementation(
        CORBA_Object _caller,
        const IVMnet_handle_t handle,
        idl4_server_environment *_env)
{
    NetIface *iface = get_iface_map()->get_iface(handle);
    if( !iface ) {
	CORBA_exception_set( _env, ex_IVMnet_invalid_handle, NULL);
	return;
    }

    // TODO: security: validate the caller

    // Spin on the device lock.
    iface->get_driver()->spin_lock_device();

    iface->activate();

    // Release the device lock, and dispatch missed events.
    if( iface->get_driver()->unlock_device() )
	iface->get_driver()->irq_handler( _caller );
}
IDL4_PUBLISH_IVMNET_CONTROL_START(IVMnet_Control_start_implementation);


IDL4_INLINE void IVMnet_Control_stop_implementation(
        CORBA_Object _caller,
        const IVMnet_handle_t handle,
        idl4_server_environment *_env)
{
    UNIMPLEMENTED();
}
IDL4_PUBLISH_IVMNET_CONTROL_STOP(IVMnet_Control_stop_implementation);


IDL4_INLINE void IVMnet_Control_update_stats_implementation(
        CORBA_Object _caller,
        const IVMnet_handle_t handle,
        idl4_server_environment *_env)
{
    UNIMPLEMENTED();
}
IDL4_PUBLISH_IVMNET_CONTROL_UPDATE_STATS(IVMnet_Control_update_stats_implementation);


IDL4_INLINE void IVMnet_Control_run_dispatcher_implementation(
        CORBA_Object _caller, 
        const IVMnet_handle_t handle,
        idl4_server_environment *_env)
{   
    NetIface *iface = get_iface_map()->get_iface(handle);
    if( !iface ) {
	CORBA_exception_set( _env, ex_IVMnet_invalid_handle, NULL);
	return;
    }

    // TODO: security: validate the caller

    iface->get_driver()->send_handler( _caller );
}           
IDL4_PUBLISH_IVMNET_CONTROL_RUN_DISPATCHER(IVMnet_Control_run_dispatcher_implementation);
IDL4_PUBLISH_IVMNET_EVENTS_RUN_DISPATCHER(IVMnet_Control_run_dispatcher_implementation);



IDL4_INLINE void IVMnet_Control_register_dp83820_tx_ring_implementation(
	CORBA_Object _caller,
	const IVMnet_handle_t handle,
	const L4_Word_t client_paddr,
	const L4_Word_t ring_size_bytes,
	const L4_Word_t has_extended_status,
        idl4_server_environment *_env)
{
    NetIface *iface = get_iface_map()->get_iface(handle);
    if( !iface ) {
	CORBA_exception_set( _env, ex_IVMnet_invalid_handle, NULL);
	return;
    }

    // TODO: security: validate the caller

    if( !has_extended_status ) {
	printf( "The client must use the dp83820 extended status.  Fatal error.\n" );
	CORBA_exception_set( _env, ex_IVMnet_no_memory, NULL );
	return;
    }



    L4_Fpage_t fp_req = L4_Fpage( client_paddr, ring_size_bytes );
    iface->dp83820.area = L4_Fpage( 
	    get_valloc()->alloc_aligned( L4_SizeLog2(fp_req) ),
	    ring_size_bytes );

    // Request the client pages from the resourcemon
    idl4_fpage_t fp;
    CORBA_Environment req_env = idl4_default_environment;
    idl4_set_rcv_window( &req_env, iface->dp83820.area );
    IResourcemon_request_client_pages( get_resourcemon_tid(),
	    &_caller, fp_req.raw, &fp, &req_env );

    if( req_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &req_env );
	CORBA_exception_set( _env, ex_IVMnet_no_memory, NULL );
	// TODO: release resources
	PANIC( "unimplemented" );
	return;
    }

    iface->dp83820.ring_cnt = ring_size_bytes / sizeof(IVMnet_dp83820_descriptor_t);
    iface->dp83820.client_phys = client_paddr;
    iface->dp83820.dirty = NULL;
    iface->dp83820.ring_start = L4_Address(iface->dp83820.area) + 
	((L4_Size(iface->dp83820.area)-1) & client_paddr);
    printf( "dp83820 tx ring, client %p, internal %p\n",
	    client_paddr, iface->dp83820.ring_start );
}
IDL4_PUBLISH_IVMNET_CONTROL_REGISTER_DP83820_TX_RING(IVMnet_Control_register_dp83820_tx_ring_implementation);


void IVMnet_Control_discard( void )
{
    printf( "net: discarded IPC message\n" );
}

void IVMnet_Events_discard( void )
{
    printf( "net: discarded IPC message\n" );
}


#/****************************************************************************
 *
 *  Event Threads
 *
 ****************************************************************************/

#if defined(CONFIG_SMP)
void event_thread( void *cpu_param )
    // The event thread.  It dispatches only event messages.  For use in
    // SMP.
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long int cnt;
    // Use static declarations so that we needn't allocate stack space.
    static void *l4net_vtable[] = INET_EVENTS_DEFAULT_VTABLE;
    static void *l4net_ktable[] = INET_EVENTS_DEFAULT_KTABLE;

    // Synchronous thread migration.
    if( !L4_Set_ProcessorNo(L4_Myself(), (int)cpu_param) )
	L4_KDB_Enter( "lxserv::net: cpu migration" );

    // Associate the interrupts *after* thread migration.
    manager.associate_thread_interrupts();

    // Allocate message buffers.
    idl4_msgbuf_init( &msgbuf );
    for( cnt = 0; cnt < INET_CONTROL_STRBUF_SIZE; cnt++ )
	idl4_msgbuf_add_buffer( &msgbuf, smalloc(8000), 8000 );

    // The message loop.
    while( 1 ) {
	partner = L4_nilthread;
	msgtag.raw = 0;
	cnt = 0;

	while( 1 )
	{
	    idl4_msgbuf_sync( &msgbuf );
	    idl4_reply_and_wait( &partner, &msgtag, &msgbuf, &cnt );
	    if( idl4_is_error(&msgtag) )
		break;
	    if( L4_IpcXcpu(msgtag) )
		nprintf( 1, "\nlxserv::net: xcpu IPC!\n" );

	    if( idl4_is_kernel_message(msgtag) )
		idl4_process_request( &partner, &msgtag, &msgbuf, &cnt, l4net_ktable[idl4_get_kernel_message_id(msgtag) & INET_CONTROL_KID_MASK] );
	    else
		idl4_process_request( &partner, &msgtag, &msgbuf, &cnt, l4net_vtable[idl4_get_function_id(&msgtag) & INET_CONTROL_FID_MASK] );
	}

	nprintf( 1, "\nlxserv::net: message error, msgtag: %x, "
		 "error code: %08x, partner: %x\n", 
		 msgtag.raw, L4_ErrorCode(), partner.raw );
	L4_KDB_Enter( "l4net message error" );
    }
}
#endif	/* CONFIG_SMP */

void control_thread( void *param, hthread_t *htread )
    // The primary message loop thread.  It dispatches event and control
    // messages.
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long int cnt;
    char dev_name[16];
    char addr_buf[6];
    static void *IVMnet_Control_vtable[] = IVMNET_CONTROL_DEFAULT_VTABLE;
    static void *IVMnet_Control_ktable[] = IVMNET_CONTROL_DEFAULT_KTABLE;

    printf( "lxserv::net: starting control thread message loop on cpu %u\n",
	    (word_t)param );
    manager.associate_thread_interrupts();

    idl4_msgbuf_init( &msgbuf );
    idl4_msgbuf_add_buffer( &msgbuf, dev_name, sizeof(dev_name) );
    idl4_msgbuf_add_buffer( &msgbuf, addr_buf, sizeof(addr_buf) );

    // The message loop.
    while( 1 )
    {
	partner = L4_nilthread;
	msgtag.raw = 0;
	cnt = 0;

	while( 1 )
	{
	    idl4_msgbuf_sync( &msgbuf );
	    idl4_reply_and_wait( &partner, &msgtag, &msgbuf, &cnt );
	    if( idl4_is_error(&msgtag) )
		break;
	    if( L4_IpcXcpu(msgtag) )
		nprintf( 1, "\nlxserv::net: xcpu IPC!\n" );

	    if( idl4_is_kernel_message(msgtag) )
		idl4_process_request( &partner, &msgtag, &msgbuf, &cnt,
			IVMnet_Control_ktable[idl4_get_kernel_message_id(msgtag) & IVMNET_CONTROL_KID_MASK] );
	    else
		idl4_process_request( &partner, &msgtag, &msgbuf, &cnt, 
		      	IVMnet_Control_vtable[idl4_get_function_id(&msgtag) & IVMNET_CONTROL_FID_MASK] );
	}

	nprintf( 1, "\nlxserv::net: message error, msgtag: %x, "
		 "error code: %08x, partner: %x\n", 
		 msgtag.raw, L4_ErrorCode(), partner.raw );
	L4_KDB_Enter( "l4net message error" );
    }

}

/****************************************************************************
 *
 *  Initialization
 *
 ****************************************************************************/

L4_ThreadId_t net_start_threads( void )
    // Create server threads for each CPU.
{
    L4_KernelInterfacePage_t *kip =
	(L4_KernelInterfacePage_t *)L4_GetKernelInterface();
    int cpu, cpu_cnt = kip->ProcessorInfo.X.processors + 1;

    // We index the cpu map by L4_ProcessorNo(), and thus perform no
    // runtime range checking.  So cfg_max_cpu had better be larger than
    // (or equal to) the number of CPUs in the system.
    ASSERT( cpu_cnt <= cfg_max_cpu );

    // Init net_cpu_map
    for( cpu = 0; cpu < cfg_max_cpu; cpu++ )
	net_cpu_map[ cpu ] = L4_nilthread;

    // Init the devices before they can be given events.
    if( !manager.open_all_devices() )
	printf( "e1000: failed to open all devices\n" );
    // Assign interrupts *before* thread start.
    manager.assign_interrupt_cpus( cpu_cnt );

    // The main driver thread.  It starts on our current cpu.
    cpu = L4_ProcessorNo();
    hthread_t *hthread = get_hthread_manager()->create_thread( 
	    get_stack_bottom(cpu), stack_size, get_max_prio(),
	    control_thread, L4_Myself(), get_thread_server_tid(), (void *)cpu );
    if( hthread == NULL )
	return L4_nilthread;
    net_cpu_map[ cpu ] = hthread->get_global_tid();
    hthread->start();
    nprintf( 2, "lxserv::net: started primary server thread %lx, on cpu %d\n",
	    net_cpu_map[ cpu ].raw, cpu );

    // The secondary driver threads.
#if defined(CONFIG_SMP)
    for( cpu = 0; cpu < cpu_cnt; cpu++ )
    {
	if( cpu == L4_ProcessorNo() )
	    continue;	// The main driver thread is on the current cpu.

	// Start the thread.
	hthread = get_hthread_manager()->create_thread(
		get_stack_bottom(cpu), stack_size, get_max_prio(),
		event_thread, L4_Myself(), get_thread_server_tid(),
		(void *)cpu );
	L4_ThreadId_t tid = hthread->get_global_tid();
	net_cpu_map[cpu] = tid;

	hthread->start();

	nprintf( 2, "lxserv::net: started secondary server thread %lx, "
		"for cpu %d\n", tid.raw, cpu );
    }
#else
    // Ensure that we have valid thread IDs in the net_cpu_map.
    for( int cpu = 0; cpu < cpu_cnt; cpu++ )
    {
	if( cpu != L4_ProcessorNo() )
	    net_cpu_map[cpu] = net_cpu_map[ L4_ProcessorNo() ];
    }
#endif

    // Register with the locator
    if( !resourcemon_register_service(UUID_IVMnet, net_cpu_map[L4_ProcessorNo()]) )
	printf( "Error registering the net server with the locator.\n" );

    return net_cpu_map[L4_ProcessorNo()]; // Return the main thread ID.
}

L4_ThreadId_t net_init( void )
    // The initial entry point for the network driver.  It searches
    // for devices, initializes everything, and starts server threads.
    // It returns the thread ID of the primary control thread.
{
    ASSERT( sizeof(LanAddr) == 6 );
    ASSERT( sizeof(LanHeader) == 14 );
    ASSERT( sizeof(E1000TxDesc) == sizeof(e1000_tx_desc) );
    ASSERT( sizeof(E1000RxDesc) == sizeof(e1000_rx_desc) );
    ASSERT( sizeof(NetClientShared) == KB(8) );
    ASSERT( sizeof(NetClientDesc) == sizeof(IVMnet_dp83820_descriptor_t) );
    ASSERT( sizeof(IVMnet_dp83820_descriptor_t) == 16 );

    ASSERT( ((word_t)get_client_shared(0) & PAGE_MASK) == (word_t)get_client_shared(0) );
    ASSERT( ((word_t)get_server_shared(0) & PAGE_MASK) == (word_t)get_server_shared(0) );

    manager.scan_devices();
    if( manager.get_tot_devices() <= 0 )
	return L4_nilthread;

#if defined(CONFIG_PACKET_REMAP)
    get_page_tank()->init( get_memsrv(), 256*manager.get_tot_devices() );
    get_page_tank_unmapper()->init();
#endif

    return net_start_threads();
}


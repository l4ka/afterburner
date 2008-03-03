/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/dp83820.cc
 * Description:   The backend of the DP83820 model, which connects to the
 *                L4Ka device-driver reuse infrastructure.
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

#include <l4/ipc.h>
#include <l4/schedule.h>

#include INC_ARCH(cpu.h)
#include INC_ARCH(bitops.h)
#include INC_ARCH(cycles.h)
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(resourcemon.h)

#include INC_WEDGE(message.h)
#include <device/dp83820.h>
#include <string.h>
#include <burn_counters.h>

#include "L4VMnet_idl_client.h"
#include "lanaddress_idl_client.h"

DECLARE_BURN_COUNTER(flush_max);
DECLARE_BURN_COUNTER(flush_max_delay);

bool l4ka_allocate_lan_address( u8_t lanaddress[6] )
{
    cpu_t &cpu = get_cpu();
    CORBA_Environment ipc_env = idl4_default_environment;
    lanaddress_t reply_addr;
    L4_ThreadId_t lanaddress_tid;

    if( !l4ka_server_locate(UUID_ILanAddress, &lanaddress_tid) )
	return false;

    bool irq_save = cpu.disable_interrupts();
    ILanAddress_generate_lan_address( lanaddress_tid, &reply_addr, &ipc_env );
    cpu.restore_interrupts( irq_save );
 
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	return false;
    }

    memcpy( lanaddress, reply_addr.raw, 6 );
    return true;
}

bool l4ka_vmarea_get( L4_Word_t req_log2size, L4_Fpage_t *fp )
{
    // TODO: build a memory allocator.
    // This must be aligned, and backed by physical memory from the wedge's
    // physical memory allocation.
    word_t end_vaddr = get_vcpu().get_wedge_end_static();
    *fp = L4_FpageLog2( end_vaddr, req_log2size );
    if( L4_Address(*fp) < end_vaddr ) {
	word_t next_addr = L4_Address(*fp) + L4_Size(*fp);
	*fp = L4_FpageLog2( next_addr, req_log2size );
	ASSERT( L4_Address(*fp) + L4_Size(*fp) < get_vcpu().get_wedge_end_vaddr() );
    }
    return true;
}

static void 
l4ka_net_rcv_thread_prepare(l4ka_net_rcv_group_t *group, dp83820_t *dp83820)
{
    vcpu_t &vcpu = get_vcpu();

    while( 1 )
    {
	// Do we have enough buffers?
	if( group->ring.dirty() >= IVMnet_rcv_buffer_cnt )
	    return;

	// Claim buffers from the guest OS.
	u16_t needed = IVMnet_rcv_buffer_cnt - group->ring.dirty();
	while( needed ) 
	{
	    dp83820_desc_t *desc = dp83820->claim_next_rx_desc();
	    if( !desc )
		break;	// Insufficient buffers.
	    
	    ASSERT( desc->device_rx_own() );
	    group->desc_ring[group->ring.start_free] = desc;
	    group->ring.start_free = 
		(group->ring.start_free + 1) % group->ring.cnt;
	    needed--;
	} 
	
	if( 0 == needed )
	{
	    dprintf(debug_dp83820_rx, "Receive group %d claimed enough receive buffers\n", group->group_no);
	    return;
	}

	if( group->ring.dirty() > 0 )
	    return; // Use our remaining buffers.

	// Must wait for buffers.
	dprintf(debug_dp83820_rx,  "Receive group %d waiting for %d buffers\n", group->group_no, needed);
	
	dp83820->backend.client_shared->receiver_tids[ group->group_no ] = L4_Myself();

	word_t irq;
	L4_MsgTag_t msg_tag;
	L4_ThreadId_t dummy;
	group->waiting = true;
	
	if( dp83820->backend_rx_async_idle(&irq) )
	{
	    // TODO: only send the idle IRQ when the buffers are empty.
	    // Otherwise, the driver resets the buffers, which screws
	    // up the ones we have already claimed.
	    dprintf(debug_dp83820_rx, "Sending idle IRQ (%d) from group %d\n", irq, group->group_no);
	    
	    msg_virq_build( irq );
	    
	    if( vcpu.in_dispatch_ipc() ) 
	    {
		msg_tag = L4_Ipc( vcpu.main_ltid, vcpu.main_ltid, 
			L4_Timeouts(L4_ZeroTime,L4_Never), &dummy);
		
		if( L4_IpcFailed(msg_tag) && ((L4_ErrorCode()&1) == 0) ) 
		{
		    // Send error to the main dispatch loop.  Resend to the
		    // IRQ loop.
		    msg_virq_build( irq );
		    msg_tag = L4_Ipc( vcpu.irq_ltid, vcpu.main_ltid, 
			    L4_Timeouts(L4_Never, L4_Never), &dummy );
		}
	    }
	    else 
	    {
		msg_tag = L4_Ipc( vcpu.irq_ltid, vcpu.main_ltid, 
				  L4_Timeouts(L4_Never,L4_Never), &dummy );
	    }
	}
	else 
	{
	    msg_tag = L4_Receive(vcpu.main_ltid);
	}

	if( L4_IpcFailed(msg_tag) )
	    printf( "dp83820 rx idle IPC failed\n");
    }
}

static void l4ka_net_rcv_thread_wait( 
	l4ka_net_rcv_group_t *group, dp83820_t *dp83820 )
{
    L4_StringItem_t string_item;
    L4_MsgTag_t msg_tag;
    L4_ThreadId_t server_tid;
    L4_Word16_t i, pkt;
    l4ka_net_ring_t *ring = &group->ring;
    dp83820_desc_t *desc;
    L4_Word_t transferred_bytes = ~0; // Absurdly high value.

    // Install string item acceptors for each packet buffer.
    pkt = ring->start_dirty;
    for( i = 0; i < IVMnet_rcv_buffer_cnt; i++ )
    {
	if( pkt == ring->start_free )
	    break;

	desc = group->desc_ring[ pkt ];
	ASSERT( desc );
	pkt = (pkt + 1) % ring->cnt;

	ASSERT( desc->device_rx_own() );
	ASSERT( desc->bufptr );

	string_item = L4_StringItem( desc->size, 
		(void *)backend_phys_to_virt(desc->bufptr) );
	if( (pkt != ring->start_free) && (i < (IVMnet_rcv_buffer_cnt-1)) )
		string_item.X.C = 1; // More buffers coming.

	// Write the message registers.
	L4_LoadBRs( i*2 + 1, 2, string_item.raw );
    }

    // Wait for packets from a valid sender.
    dprintf(debug_dp83820_rx, "Waiting for string copy, group %d\n", group->group_no);
    ASSERT( L4_XferTimeouts() == L4_Timeouts(L4_Never, L4_Never) );
    
    dp83820->backend.client_shared->receiver_tids[ group->group_no ] = L4_Myself();
    L4_Accept( L4_StringItemsAcceptor );
    
    msg_tag = L4_Wait( &server_tid );
    
    if( EXPECT_FALSE(L4_IpcFailed(msg_tag))) 
	transferred_bytes = L4_ErrorCode() >> 4;

    dprintf(debug_dp83820_rx, "String copy received from %t, group %d, items %d bytes %d\n",
	    server_tid, group->group_no, L4_TypedWords(msg_tag)/2, transferred_bytes);

    // Swallow the received packets.
    bool desc_irq = false;
    for( i = L4_UntypedWords(msg_tag) + 1;
	 (transferred_bytes && ((i+1)) <= (L4_Word16_t) (L4_UntypedWords(msg_tag) + L4_TypedWords(msg_tag)));
	 i += 2 )
    {
	// Read the message registers.
	L4_StoreMRs( i, 2, string_item.raw );
	// Release ownership of the descriptors.
	desc = group->desc_ring[ ring->start_dirty ];
	// TODO: handle invalid IPC messages.
	ASSERT( L4_IsStringItem(&string_item) );
	ASSERT( desc->device_rx_own() );

	if( desc->cmd_status.rx.intr )
	    desc_irq = true;
	desc->cmd_status.rx.ok = 1;
	desc->cmd_status.rx.more = 0;
	// TODO: don't assume that we have an extended descriptor
	desc->extended_status.ip_pkt = 1; // Imply that checksum is complete.
	desc->size = string_item.X.string_length;
	transferred_bytes -= string_item.X.string_length;
	desc->device_rx_release();
	group->desc_ring[ ring->start_dirty ] = NULL;

	ring->start_dirty = (ring->start_dirty + 1) % ring->cnt;
    }

    // Update the device IRQ status.
    if( desc_irq )
	dp83820->set_rx_desc_irq();
    dp83820->set_rx_ok_irq();
    dp83820->backend_raise_async_irq( );
}

struct net_rcv_thread_params_t
{
    l4ka_net_rcv_group_t *group;
    dp83820_t *dp83820;
    vcpu_t *vcpu;
};

static void l4ka_net_rcv_thread( void *param, hthread_t *hthread )
{
    // Thread-specific data.
    net_rcv_thread_params_t *tlocal = 
	(net_rcv_thread_params_t *)hthread->get_tlocal_data();
    l4ka_net_rcv_group_t *group = tlocal->group;
    dp83820_t *dp83820 = tlocal->dp83820;
    // Set our thread's vcpu object (and kill the hthread tlocal data).
    
    vcpu_t *vcpu_param =  (vcpu_t *) param;
    ASSERT(param);
    set_vcpu(*vcpu_param);
    
    // Set our thread's exception handler.
    L4_Set_ExceptionHandler( get_vcpu().monitor_gtid );
    L4_Set_XferTimeouts( L4_Timeouts(L4_Never, L4_Never) );

    dprintf(debug_dp83820_rx, "Entering receiver thread, TID %t group %d dp83820 %x\n",
		L4_Myself(), group->group_no, dp83820);

    for (;;)
    {
	l4ka_net_rcv_thread_prepare( group, dp83820);
	l4ka_net_rcv_thread_wait( group, dp83820 );
    }
}

bool dp83820_t::backend_rx_async_idle( word_t *irq )
{
    if( (dp83820_backend_t::nr_rcv_group > 1)
    	    &&!atomic_dec_and_test(&backend.idle_count) )
	return false;

    // Trigger the rx idle interrupt.
    set_rx_idle_irq();
    if( !*irq_pending && (regs[ISR] & regs[IMR]) && regs[IER] ) {
	*irq_pending = true;
	*irq = get_irq();
	return true;
    }
    return false;
}

void dp83820_t::backend_cancel_rx_async_idle()
{
    if( dp83820_backend_t::nr_rcv_group > 1 )
	atomic_inc( &backend.idle_count );
}

void dp83820_t::backend_init()
{
    if( backend_connect )
	return;	// We are already connected to the network server.

    dprintf(debug_dp83820_init, "dp83820 L4Ka init\n");
    if( !l4ka_allocate_lan_address(lan_address) ) {
	printf( "Failed to generate a virtual LAN address.\n");
	return;
    }
	
    if( !l4ka_server_locate(UUID_IVMnet, &backend.server_tid) ) {
	printf( "Failed to locate a network server.\n");
	return;
    }
    dprintf(debug_dp83820_init, "Network server TID %t\n", backend.server_tid);

    // Allocate an oversized shared window region, to adjust for alignment.
    // We receive two fpages in the area.  We will align to the largest fpage.
    word_t size = L4_Size( L4_Fpage(0, sizeof(IVMnet_client_shared_t)) );
    word_t size2 = L4_Size( L4_Fpage(0, sizeof(IVMnet_server_shared_t)) );
    word_t log2size = L4_SizeLog2( L4_Fpage(0, size+size2) );
    if( !l4ka_vmarea_get( log2size, &backend.shared_window) ) {
	printf( "Failed to allocate a virtual address region for the "
	    "shared network window.\n");
	return;
    }
    dprintf(debug_dp83820_init, "Shared network window %x size %08d\n", 
		L4_Address(backend.shared_window), L4_Size(backend.shared_window));

    char target_device[10] = "eth1";
    cmdline_key_search( "dp83820=", target_device, sizeof(target_device) );

    if (!strncmp(target_device, "off", 3))
    {
	dprintf(debug_dp83820_init, "dp83820 turned off\n");
	return;
    }
    // Attach to the server, and request the shared mapping area.
    idl4_fpage_t idl4_mapping, idl4_server_mapping;
    CORBA_Environment ipc_env = idl4_default_environment;
    bool irq_save = get_cpu().disable_interrupts();
    idl4_set_rcv_window( &ipc_env, backend.shared_window );
    IVMnet_Control_attach( backend.server_tid, 
	    target_device, (char *)lan_address,
	    &backend.ivmnet_handle, &idl4_mapping, &idl4_server_mapping,
	    &ipc_env );
    get_cpu().restore_interrupts( irq_save );
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	printf( "Failed to attach to the network server.\n");
	return;
    }
    dprintf(debug_dp83820_init, "Shared region at base %x size %08d\n",
	    idl4_fpage_get_base(idl4_mapping), L4_Size(idl4_fpage_get_page(idl4_mapping)));
    dprintf(debug_dp83820_init,  "Server status region at base %x size %08d\n", 
	    idl4_fpage_get_base(idl4_server_mapping), L4_Size(idl4_fpage_get_page(idl4_server_mapping)));
    
    word_t window_base = L4_Address( backend.shared_window );
    backend.client_shared = (IVMnet_client_shared_t *)
	(window_base + idl4_fpage_get_base(idl4_mapping));
    backend.server_status = (IVMnet_server_shared_t *)
	(window_base + idl4_fpage_get_base(idl4_server_mapping));

    // IRQ stuff.
    backend.client_shared->client_irq = get_irq();
    backend.client_shared->client_main_tid = get_vcpu().main_gtid;
    backend.client_shared->client_irq_tid = get_vcpu().irq_gtid;
    backend.client_shared->client_irq_pending = *irq_pending;
    irq_pending = (volatile word_t *)&backend.client_shared->client_irq_pending;

    // Switch the registers.
    ASSERT( regs == local_regs );
    memcpy( backend.client_shared->dp83820_regs, regs, last*4 );
    regs = backend.client_shared->dp83820_regs;

    // Receive group stuff.
    backend.group_count = 0;
    backend.idle_count = 0;
    backend.client_shared->receiver_cnt = 0;
    for( word_t i = 0; 
	    i < dp83820_backend_t::nr_rcv_group && i < IVMnet_max_receiver_cnt;
	    i++ )
    {
	// Init the receive thread.
	l4ka_net_rcv_group_t &rcv_group = backend.rcv_group[i];
	rcv_group.group_no = i;
	rcv_group.dev_tid = L4_nilthread;
	rcv_group.hthread = NULL;
	rcv_group.ring.cnt = l4ka_net_rcv_group_t::nr_desc;
	rcv_group.ring.start_free = 0;
	rcv_group.ring.start_dirty = 0;
	memzero( rcv_group.desc_ring, sizeof(rcv_group.desc_ring) );
	rcv_group.waiting = false;

	// Create the receive thread.
	vcpu_t &vcpu = get_vcpu();
	net_rcv_thread_params_t params;
	params.group = &rcv_group;
	params.dp83820 = this;
	params.vcpu = &vcpu;
	
	rcv_group.hthread = get_hthread_manager()->create_thread( 
	    &vcpu, (L4_Word_t)rcv_group.thread_stack, sizeof(rcv_group.thread_stack),
	    resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER, l4ka_net_rcv_thread, 
	    L4_Pager(),  &vcpu, &params, sizeof(params) );

	if( rcv_group.hthread == NULL ) {
	    printf( "Failed to start a network receiver thread.\n");
	    continue;
	}
	backend.group_count++;
	backend.idle_count++;

	// Start the receive thread.
	rcv_group.dev_tid = rcv_group.hthread->get_global_tid();
	rcv_group.hthread->start();

	// Tell the server about the receive thread.
	backend.client_shared->receiver_cnt++;
    }

    if( backend.group_count == 0 )
	return;	// We failed to start driver threads.

    // Tell the server that we are ready to handle packets.
    ipc_env = idl4_default_environment;
    irq_save = get_cpu().disable_interrupts();
    IVMnet_Control_start( backend.server_tid, backend.ivmnet_handle, &ipc_env);
    get_cpu().restore_interrupts( irq_save );
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	printf( "Failed to start the network server.\n");
	return;
    }

    backend_connect = true;
    get_intlogic().add_virtual_hwirq( get_irq() );
    get_intlogic().clear_hwirq_squash( get_irq() );
	
}

INLINE L4_MsgTag_t empty_ipc( 
	L4_MsgTag_t mr0,
	L4_ThreadId_t to, 
	L4_ThreadId_t from_specifier, 
	L4_Word_t timeouts, 
	L4_ThreadId_t *from )
{
    L4_ThreadId_t result;
    L4_Word_t mr1, mr2;

    __asm__ __volatile__ (
	    __L4_SAVE_REGS
	    "call __L4_Ipc\n"
	    "movl %%ebp, %%ecx\n"
	    __L4_RESTORE_REGS

	    : /* outputs */
	    "=S" (mr0),
	    "=a" (result),
	    "=d" (mr1),
	    "=c" (mr2)

	    : /* inputs */
	    "S" (mr0),
	    "a" (to.raw),
	    "D" (__L4_X86_Utcb()),
	    "c" (timeouts),
	    "d" (from_specifier.raw)
	    );

    if( !L4_IsNilThread(from_specifier) )
	*from = result;
    return mr0;
}

void dp83820_t::rx_enable()
{
    for( word_t i = 0; i < backend.group_count; i++ )
    {
	if( !backend.rcv_group[i].waiting )
	    continue;

	// Wake a receive group.
	dprintf(debug_dp83820_rx, "Waking group %d\n", i);

	L4_ThreadId_t dummy;
	L4_MsgTag_t tag;
	tag.raw = 0;

	backend.rcv_group->waiting = false;
	backend_cancel_rx_async_idle();

#if 0
	cpu_t &cpu = get_cpu();
	bool irq_save = cpu.disable_interrupts();
	L4_Set_MsgTag( tag );
	tag = L4_Reply( backend.rcv_group[i].dev_tid );
	cpu.restore_interrupts( irq_save );
#else
	tag = empty_ipc( tag, backend.rcv_group[i].dev_tid, L4_nilthread, 
		L4_Timeouts(L4_ZeroTime, L4_Never), &dummy );
#endif

	if( L4_IpcFailed(tag) )
	    printf( "dp83820 IPC wake error\n");
    }
}

void dp83820_t::tx_enable()
{
    // In case the server preempts us and runs its packet processor, let
    // it know that we have pending packets.
    // TODO: perhaps we should look for an empty transmit ring before
    // sending a request to the server.  But a device driver is unlikely
    // to perform a TX enable with an empty transmit ring.
    backend.server_status->irq_status |= 1;

    if( !flush_cnt )
	flush_start = get_cycles();
    atomic_inc( &flush_cnt );

//    	backend_flush( true ); // Uncomment for immediate transmit only.
}

void dp83820_t::backend_flush( bool going_idle )
{
    // Every iret and idle operation invokes this flush op.  We must
    // be sure to determine when a flush is actually necessary.
    // (1) If a flush is already in progress, or no work has been scheduled,
    //     then flush_cnt is zero.
    // (2) If the network server has a pending irq, then we have no reason
    //     to signal the server again, because the server is already
    //     scheduled to wake and process pending work.
    // TODO: in case the server already transmitted our packets, we should
    // cancel our notification.

    if( !flush_cnt )
	return;
    backend.server_status->irq_status |= 1;
    if( backend.server_status->irq_pending ) {
	return;
    }
    dprintf(debug_dp83820_tx, "dp83820 backend flush %d packets\n", flush_cnt);

    backend.server_status->irq_pending = true;

    vcpu_t &vcpu = get_vcpu();
    L4_MsgTag_t msg_tag, result_tag;
    ASSERT( L4_MyLocalId() == vcpu.main_ltid );

    bool irq_save = vcpu.cpu.disable_interrupts();
    {
	MAX_BURN_COUNTER(flush_max, flush_cnt);
	MAX_BURN_COUNTER(flush_max_delay, get_cycles() - flush_start );
	flush_cnt = 0;
    
	if( L4_IsNilThread(backend.client_shared->server_main_tid) )
	{
	    CORBA_Environment ipc_env = idl4_default_environment;
	    IVMnet_Control_run_dispatcher( backend.server_tid, 
		    backend.ivmnet_handle, &ipc_env );
	    if( ipc_env._major != CORBA_NO_EXCEPTION )
	    {
		CORBA_exception_free( &ipc_env );
		printf( "dp83820 TX IPC err\n");
	    }
	}
	else
	{
	    msg_tag.raw = 0; msg_tag.X.label = msg_label_virq; msg_tag.X.u = 1;
	    L4_Set_MsgTag( msg_tag );
	    L4_LoadMR( 1, backend.client_shared->server_irq );
	    result_tag = L4_Call( backend.client_shared->server_irq_tid );
	}
    }
    vcpu.cpu.restore_interrupts( irq_save );
}

void dp83820_t::backend_raise_async_irq()
{
    if( *irq_pending || !((regs[ISR] & regs[IMR]) && regs[IER]) )
	return;
    *irq_pending = true;

    vcpu_t &vcpu = get_vcpu();
    ASSERT( L4_MyLocalId() != vcpu.main_ltid );

    dprintf(debug_dp83820_tx, "dp83820 backend raise async irq to %t\n", vcpu.irq_ltid);

    msg_virq_build( get_irq() );
    if( vcpu.in_dispatch_ipc() ) 
    {
	L4_MsgTag_t tag = L4_Reply( vcpu.main_ltid );
	if( L4_IpcFailed(tag) ) {
	    msg_virq_build( get_irq() );
	    L4_Call( vcpu.irq_ltid );
	}
    }
    else
	L4_Call( vcpu.irq_ltid );
}

void dp83820_t::txdp_absorb()
{
    // Calculate the size of the TX descriptor ring.  We assume that each
    // descriptor lies next to the prior descriptor in memory.

    dp83820_desc_t *first = get_tx_desc_virt();

    dprintf(debug_dp83820_init, "dp83820 start tx desc ring %x\n", first);
    if( has_extended_status() )
	dprintf(debug_dp83820_init, "dp83820 configured for extended status.\n");

    dp83820_desc_t *desc = first;
    while( 1 ) {
	dp83820_desc_t *link = get_tx_desc_next(desc);
	if( !link || (link == first) )
	    break;
	desc = link;
    }

    word_t desc_size = sizeof(dp83820_desc_t);
    if( !has_extended_status() )
	desc_size -= 4;

    word_t size = ((desc - first) + 1) * desc_size;
    
    if( get_tx_desc_next(desc) != first )
	PANIC( "The dp83820 isn't configured for a transmit ring." );

    cpu_t &cpu = get_cpu();
    CORBA_Environment ipc_env = idl4_default_environment;

    bool irq_save = cpu.disable_interrupts();
    IVMnet_Control_register_dp83820_tx_ring( backend.server_tid,
	    backend.ivmnet_handle, get_txdp(), size, has_extended_status(),
	    &ipc_env );
    cpu.restore_interrupts( irq_save );

    dprintf(debug_dp83820_init, "dp83820 end tx desc ring %x size %d number %d\n",
	    desc, size, size/desc_size);

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	PANIC( "Failed to share the dp83820 tx descriptor ring.");
    }
}


void dp83820_backend_t::handle_pfault( word_t fault_addr )
{
    ASSERT( L4_MyLocalId() == get_vcpu().monitor_ltid );

    idl4_fpage_t idl4_mapping, idl4_server_mapping;
    CORBA_Environment ipc_env = idl4_default_environment;

    idl4_set_rcv_window( &ipc_env, shared_window );

    IVMnet_Control_reattach( server_tid, ivmnet_handle,
	    &idl4_mapping, &idl4_server_mapping, &ipc_env );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	PANIC( "Failed to reattach to the dp83820 network server." );
    }
}


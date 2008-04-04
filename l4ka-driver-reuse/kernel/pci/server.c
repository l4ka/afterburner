/*********************************************************************
 *
 * Copyright (C) 2004-2006,  University of Karlsruhe
 *
 * File path:	l4ka-driver-reuse/kernel/pci/server.c
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
#include <linux/version.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <l4/types.h>
#include <l4/space.h>
#include <l4/kdebug.h>

#include <glue/wedge.h>
#include <glue/vmserver.h>
#include <glue/thread.h>

#include "common.h"
#include "L4VMpci_idl_server.h"
#include "L4VMpci_idl_reply.h"


#if defined(CONFIG_X86_IO_APIC)
#include <acpi/acpi.h>
#include <linux/acpi.h>
#endif

#if defined(PREFIX)
#undef PREFIX
#endif
#include "server.h"


#define L4VMPCI_DEV_INIT(name) { LIST_HEAD_INIT(name->devs), NULL, 0UL }

int L4VMpci_debug_level = 3;

int L4VMpci_server_irq = 0;
MODULE_PARM( L4VMpci_server_irq, "i" );

/**
 * Single instance of server context.
 */
static L4VMpci_server_t L4VMpci_server = {
    server_tid: L4_nilthread,
    clients: LIST_HEAD_INIT(L4VMpci_server.clients)
};


static void L4VMpci_deliver_irq(L4_Word_t irq_flags)
{
    L4VM_server_deliver_irq(L4VMpci_server.my_irq_tid, L4VMpci_server_irq,  irq_flags,
			    &L4VMpci_server.irq_status, L4VMpci_server.irq_mask, &L4VMpci_server.irq_pending);
    
}

static L4_Word16_t L4VMpci_cmd_allocate_bottom(void)
{
    return L4VM_server_cmd_allocate( &L4VMpci_server.bottom_half_cmds, L4VMpci_server.my_irq_tid, 
				     L4VMpci_server_irq, L4VMPCI_IRQ_BOTTOM_HALF_CMD, 
				     &L4VMpci_server.irq_status, L4VMpci_server.irq_mask,
				     &L4VMpci_server.irq_pending);

}


/*********************************************************************
 *
 * client management
 *
 ********************************************************************/

static L4VMpci_client_t *
L4VMpci_server_create_client( L4VMpci_server_t *server, L4_ThreadId_t tid )
{
    L4VMpci_client_t *client;

    client = kmalloc( sizeof( L4VMpci_client_t ), GFP_KERNEL );
    if( NULL != client ) {
	client->l_server.next = &client->l_server;
	client->l_server.prev = &client->l_server;
	client->tid = tid;
	client->devs.next = &client->devs;
	client->devs.prev = &client->devs;
	list_add( &client->l_server, &server->clients );
    } else {
	printk( KERN_ERR PREFIX "unable to allocate client object, out of mem!\n" );
    }

    return client;
}

static void
L4VMpci_server_deregister_client( L4VMpci_server_t *server, L4VMpci_client_t *client )
{
    struct list_head *elem = NULL;

    list_for_each( elem, &client->devs ) {
	kfree( list_entry( elem, L4VMpci_dev_t, devs ) );
    }
    kfree( client );
}

static int
L4VMpci_client_add_device( L4VMpci_client_t *client, unsigned idx, struct pci_dev *dev, int writable )
{
    L4VMpci_dev_t *l4dev;

    l4dev = kmalloc( sizeof( L4VMpci_dev_t ), GFP_KERNEL );
    if( NULL != l4dev )
    {
	l4dev->idx = idx;
	l4dev->devs.next = &l4dev->devs;
	l4dev->devs.prev = &l4dev->devs;
	l4dev->dev = dev;
	l4dev->writable = writable;
	printk(PREFIX "adding device %02x.%02x\n", dev->bus->number, dev->devfn);
	list_add( &l4dev->devs, &client->devs );
	return 0;
    }

    return -ENOMEM;
}

static L4VMpci_client_t *
L4VMpci_server_lookup_client( L4VMpci_server_t *server, L4_ThreadId_t tid )
{
    struct list_head *elem;
    L4VMpci_client_t *client;

    list_for_each( elem, &server->clients ) {
	client = list_entry( elem, L4VMpci_client_t, l_server );
	if( L4_IsThreadEqual( client->tid, tid ) )
	    return client;
    }

    return NULL;
}

static L4VMpci_dev_t *
L4VMpci_server_lookup_device( L4VMpci_client_t *client, unsigned idx )
{
    struct list_head *elem = NULL;

    list_for_each( elem, &client->devs ) {
	L4VMpci_dev_t *dev = list_entry( elem, L4VMpci_dev_t, devs );
	if( dev->idx == idx )
	    return dev;
    }

    return NULL;
}


/*********************************************************************
 *
 * request handling
 *
 ********************************************************************/

#define idl4_default_server_environment {0,0}

static void
L4VMpci_server_register_handler( L4VM_server_cmd_t *cmd, void *data )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    int dev_num, idx;
    IVMpci_dev_id_t *devs;
    idl4_server_environment _env = idl4_default_server_environment;
    L4VMpci_client_t *client = NULL;
    int err = -ENODEV;
    struct pci_dev *dev;
    unsigned long flags;

    dev_num = params->reg.dev_num;
    devs = params->reg.devs;

    dprintk( 3, KERN_INFO PREFIX "reserving devices ");
    for( idx = 0; idx < dev_num; idx++ ) {
	dprintk( 3, "[%hx:%hx] ", devs[idx].vendor_id, devs[idx].device_id );
    }
    dprintk( 3, "\n");

    // scan all devices Linux found on the real PCI bus
    dev = NULL;
#if 0
    for( ; ; )
    {
	int add_dev = 0, writable = 1;
	dev = pci_find_device( PCI_ANY_ID, PCI_ANY_ID, dev );
	if( !dev )
	    break;

	for ( idx = 0; idx < dev_num; idx++ ) {
	    // check whether this is one of the requested devices
	    if( devs[idx].vendor_id == dev->vendor && devs[idx].device_id == dev->device ) {
		add_dev = 1;
		goto found;
	    }
	} 
	// always add bridges, but read-only
	if( dev->hdr_type == PCI_HEADER_TYPE_BRIDGE ) {
	    writable = 0;
	    add_dev = 1;
	}
	
found:
#endif

    for( idx = 0; idx < dev_num; idx++ )
    {
	dev = pci_find_device( devs[idx].vendor_id, devs[idx].device_id, NULL );

    	// create client object
       	if( NULL == client)
    	    client = L4VMpci_server_create_client( &L4VMpci_server, cmd->reply_to_tid );
       	if( NULL == client ) {
    	    printk( KERN_ERR PREFIX "failed to create client descriptor\n" );
    	    CORBA_exception_set( &_env, ex_IVMpci_unknown, NULL );
    	    goto exit;
       	}

	// add linux device to client object
	err = L4VMpci_client_add_device( client, idx, dev, TRUE );
	if( 0 != err ) {
	    printk( KERN_ERR PREFIX "failed to associate VM client with device\n" );
	    L4_KDB_Enter("ugh");
	}
    }

exit:
    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMpci_Control_register_propagate_reply( cmd->reply_to_tid, &_env );
    local_irq_restore(flags);
}

static void
L4VMpci_server_deregister_handler( L4VM_server_cmd_t *cmd, void *data )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    idl4_server_environment _env = idl4_default_server_environment;
    L4VMpci_client_t *client = NULL;
    L4_ThreadId_t tid;
    unsigned long flags;

    client = L4VMpci_server_lookup_client( server, params->dereg.tid );
    if( NULL != client ) {
	L4VMpci_server_deregister_client( server, client );
    } else {
	dprintk( 1, KERN_INFO PREFIX "unable to find specified client %p\n",
		 RAW(tid) );
	CORBA_exception_set( &_env, ex_IVMpci_invalid_client, NULL );
    }

    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMpci_Control_deregister_propagate_reply( cmd->reply_to_tid, &_env );
    local_irq_restore(flags);
}

static void
L4VMpci_server_read_handler( L4VM_server_cmd_t *cmd, void *data )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    idl4_server_environment _env = idl4_default_server_environment;
    L4VMpci_client_t *client = NULL;
    unsigned long flags;

    u32 val = 0;
    int ret_val = 0;
    u32 reg = params->r_w.reg*4 + params->r_w.offset;

    client = L4VMpci_server_lookup_client( server, cmd->reply_to_tid );
    if( NULL != client ) {
	L4VMpci_dev_t *dev;

	dev = L4VMpci_server_lookup_device( client, params->r_w.idx );
	if( NULL != dev ) {
	    struct pci_dev *pci_dev = dev->dev;

	    switch( params->r_w.len ) {
		case 1:
		    ret_val = pci_read_config_byte( pci_dev, reg, (u8*)&val );
		    break;
		case 2:
		    ret_val = pci_read_config_word( pci_dev, reg, (u16*)&val );
		    break;
		case 4:
		    ret_val = pci_read_config_dword( pci_dev, reg, &val );
		    break;

	    }
	    dprintk( 3, KERN_INFO PREFIX "client tried to read from device %02x  (%x.%x) %d/%d/%d -> %x\n",
	    	     params->r_w.idx, pci_dev->vendor, pci_dev->device, params->r_w.len, params->r_w.reg, params->r_w.offset, val);
	} else {
	    dprintk( 5, KERN_INFO PREFIX "client tried to read from invalid device\n" );
	    ret_val = 0;
	    val = 0xFFFFFFFF;
	}
    } else {
	dprintk( 3, KERN_INFO PREFIX "unable to find client %p\n",
		 RAW(cmd->reply_to_tid) );
	CORBA_exception_set( &_env, ex_IVMpci_invalid_client, NULL );
    }


    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMpci_Control_read_propagate_reply( cmd->reply_to_tid, 
	    &val, ret_val, &_env );
    local_irq_restore(flags);
}

static void
L4VMpci_server_write_handler( L4VM_server_cmd_t *cmd, void *data )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;
    L4VM_server_cmd_params_t *params = (L4VM_server_cmd_params_t *)cmd->data;
    idl4_server_environment _env = idl4_default_server_environment;
    L4VMpci_client_t *client = NULL;
    int ret_val = 0;
    unsigned long flags;
    u32 reg = params->r_w.reg*4 + params->r_w.offset;

    client = L4VMpci_server_lookup_client( server, cmd->reply_to_tid );
    if( NULL != client ) {
	L4VMpci_dev_t *dev;

	dev = L4VMpci_server_lookup_device( client, params->r_w.idx );
	if( NULL != dev ) {
	    if( 1 == dev->writable ) {
		struct pci_dev *pci_dev = dev->dev;
		switch( params->r_w.len ) {
		    case 1:
			ret_val = pci_write_config_byte( pci_dev, reg, (u8)(params->r_w.val & 0xFF) );
			break;
		    case 2:
			ret_val = pci_write_config_word( pci_dev, reg, (u16)(params->r_w.val & 0xFFFF) );
			break;
		    case 4:
			ret_val = pci_write_config_dword( pci_dev, reg, params->r_w.val );
			break;
		}
		dprintk( 1, KERN_INFO PREFIX "client tried to write to device %02x %d/%d/%d -> %x\n",
			 params->r_w.idx, params->r_w.len, params->r_w.reg, params->r_w.offset, params->r_w.val);
			    
	    } else {
		dprintk( 4, KERN_INFO PREFIX "client tried to write to read-only device\n" );
		ret_val = 0;
	    }

	} else {
	    dprintk( 5, KERN_INFO PREFIX "client tried to write to invalid device\n" );
	    ret_val = 0;
	}
    } else {
	dprintk( 1, KERN_INFO PREFIX "unable to find specified client %p\n",
		 RAW(cmd->reply_to_tid) );
	CORBA_exception_set( &_env, ex_IVMpci_invalid_client, NULL );
    }

    local_irq_save(flags);
    L4_Set_VirtualSender( server->server_tid );
    IVMpci_Control_write_propagate_reply( cmd->reply_to_tid, ret_val, &_env );
    local_irq_restore(flags);
}


/***************************************************************************
 *
 * Linux interrupt functions.
 *
 ***************************************************************************/

static void
L4VMpci_bottom_half_handler( void *data )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;

    dprintk( 5, KERN_INFO PREFIX "bottom half handler\n" );

//    ASSERT( !in_interrupt() );

    L4VM_server_cmd_dispatcher( &server->bottom_half_cmds, server,
	    server->server_tid );
}

static irqreturn_t
L4VMpci_irq_handler( int irq, void *data, struct pt_regs *regs )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;

    while( 1 )
    {
	// Outstanding events? Read them and reset without losing events.
	L4_Word_t events = irq_status_reset( &server->irq_status );
	if( !events )
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	    return;
#else
	    return IRQ_HANDLED;
#endif

	dprintk( 5, KERN_INFO PREFIX "irq handler: 0x%lx\n",
		events );

	server->irq_pending = TRUE;
//	server->server_info->irq_pending = TRUE;

	if( (events & L4VMPCI_IRQ_TOP_HALF_CMD) != 0 )
	    L4VM_server_cmd_dispatcher( &server->top_half_cmds, server,
		    server->server_tid );

	if( (events & L4VMPCI_IRQ_BOTTOM_HALF_CMD) != 0 )
	{
	    // Make sure that we tackle bottom halves outside the
	    // interrupt context!!
	    schedule_task( &server->bottom_half_task );
	}

	// Enable interrupt message delivery.
	server->irq_pending = FALSE;
//	server->server_info->irq_pending = FALSE;
    }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static int
L4VMpci_irq_pending( void *data )
{
    L4VMpci_server_t *server = (L4VMpci_server_t *)data;

    return (server->irq.status & server->irq.mask);// ||
//	server->server_info->irq_status;
}
#endif

/*********************************************************************
 *
 * module level functions
 *
 ********************************************************************/

static void 
L4VMpci_server_thread( void *data )
{
    IVMpci_Control_server();
}

static int __init
L4VMpci_server_init_module( void )
{
    int err;
    L4VMpci_server_t *server = &L4VMpci_server;

    server->irq_status = 0;
    
    server->irq_status = 0;
    server->irq_mask = ~0;
    server->irq_pending = FALSE;
    server->my_irq_tid = get_vcpu()->irq_gtid;
    server->my_main_tid = get_vcpu()->main_gtid;

    L4VM_server_cmd_init( &server->top_half_cmds );
    L4VM_server_cmd_init( &server->bottom_half_cmds );

    INIT_TQUEUE( &server->bottom_half_task,
		 L4VMpci_bottom_half_handler, server );

    printk( KERN_INFO PREFIX "initializing...\n" );

    // Allocate a virtual interrupt.
    if (L4VMpci_server_irq == 0)
    { 
#if defined(CONFIG_X86_IO_APIC)
        L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
        L4VMpci_server_irq = L4_ThreadIdSystemBase(kip) + 2;	
	acpi_register_gsi(L4VMpci_server_irq, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_LOW);

#else
	L4VMpci_server_irq = 3;
#endif
    }
    
    printk( KERN_INFO PREFIX "L4VMpci server irq %d\n", L4VMpci_server_irq );
    
    
    if( L4VMpci_server_irq >= NR_IRQS ) {
	printk( KERN_ERR PREFIX "unable to reserve a virtual interrupt.\n" );
	err = -ENOMEM;
	goto err_vmpic_reserve;
    }
    // TODO: in case of error, we don't want the irq to remain virtual.
    l4ka_wedge_add_virtual_irq( L4VMpci_server_irq );
    err = request_irq( L4VMpci_server_irq, L4VMpci_irq_handler, 0,
	    "L4VMpci", server );
    if( err < 0 ) {
	printk( KERN_ERR PREFIX "unable to allocate an interrupt.\n" );
	goto err_request_irq;
    }

    // Start the server thread.
    server->server_tid = L4VM_thread_create( GFP_KERNEL,
	    L4VMpci_server_thread, l4ka_wedge_get_irq_prio(), 
	    smp_processor_id(), NULL, 0 );
    if( L4_IsNilThread(server->server_tid) ) {
	err = -ENOMEM;
	printk( KERN_ERR PREFIX "failed to start the server thread.\n" );
	goto err_thread_create;
    }

    // Register with the locator.
    err = L4VM_server_register_location( UUID_IVMpci, server->server_tid );
    if( err == -ENODEV ) {
	printk( KERN_ERR PREFIX "failed to register with the locator.\n" );
	goto err_register;
    }
    else if( err ) {
	printk( KERN_ERR PREFIX "failed to contact the locator service.\n" );
	goto err_register;
    }

    return 0;

err_register:
err_thread_create:
    free_irq( L4VMpci_server_irq, server );
err_request_irq:
err_vmpic_reserve:
    return err;
}

static void __exit
L4VMpci_server_exit_module( void )
{
    L4_KDB_Enter("pci server exit");
}

MODULE_AUTHOR( "Stefan Goetz <sgoetz@ira.uka.de>" );
MODULE_DESCRIPTION( "L4Linux virtual PCI config space server" );
MODULE_LICENSE( "Dual BSD/GPL" );
MODULE_SUPPORTED_DEVICE( "L4 VM PCI" );
MODULE_VERSION( "monkey" );

module_init( L4VMpci_server_init_module );
module_exit( L4VMpci_server_exit_module );

#if defined(MODULE)
// Define a .afterburn_wedge_module section, to activate symbol resolution
// for this module against the wedge's symbols.
__attribute__ ((section(".afterburn_wedge_module")))
burn_wedge_header_t burn_wedge_header = {
    BURN_WEDGE_MODULE_INTERFACE_VERSION
};
#endif

/*********************************************************************
 *
 * IDL server
 *
 ********************************************************************/

IDL4_INLINE void IVMpci_Control_register_implementation(CORBA_Object _caller, const int dev_num, const IVMpci_dev_id_t *devs, idl4_server_environment *_env)

{
    L4VMpci_server_t *server = &L4VMpci_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 3, KERN_INFO PREFIX "server thread: client register\n" );

    cmd_idx = L4VMpci_cmd_allocate_bottom();
    
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];
    params->reg.dev_num = dev_num;
    params->reg.devs = (IVMpci_dev_id_t *)devs;

    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMpci_server_register_handler;

    L4VMpci_deliver_irq(L4VMPCI_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR 
IDL4_PUBLISH_IVMPCI_CONTROL_REGISTER(IVMpci_Control_register_implementation);

IDL4_INLINE void IVMpci_Control_deregister_implementation(CORBA_Object _caller, const L4_ThreadId_t *tid, idl4_server_environment *_env)

{
    L4VMpci_server_t *server = &L4VMpci_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 3, KERN_INFO PREFIX "server thread: deregister request\n" );

    cmd_idx = L4VMpci_cmd_allocate_bottom();
    
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];
    params->dereg.tid = *tid;

    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMpci_server_deregister_handler;

    L4VMpci_deliver_irq(L4VMPCI_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.
}
IDL4_PUBLISH_ATTR
IDL4_PUBLISH_IVMPCI_CONTROL_DEREGISTER(IVMpci_Control_deregister_implementation);

IDL4_INLINE int IVMpci_Control_read_implementation(CORBA_Object _caller, const int idx, const int reg, const int len, const int offset, unsigned int *val, idl4_server_environment *_env)

{
    L4VMpci_server_t *server = &L4VMpci_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 5, KERN_INFO PREFIX "server thread: read request\n" );

    cmd_idx = L4VMpci_cmd_allocate_bottom();
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];
    params->r_w.idx = idx;
    params->r_w.reg = reg;
    params->r_w.len = len;
    params->r_w.offset = offset;

    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMpci_server_read_handler;

    L4VMpci_deliver_irq(L4VMPCI_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.

    return 0;
}
IDL4_PUBLISH_ATTR
IDL4_PUBLISH_IVMPCI_CONTROL_READ(IVMpci_Control_read_implementation);

IDL4_INLINE int IVMpci_Control_write_implementation(CORBA_Object _caller, const int idx, const int reg, const int len, const int offset, const unsigned int val, idl4_server_environment *_env)

{
    L4VMpci_server_t *server = &L4VMpci_server;
    L4_Word16_t cmd_idx;
    L4VM_server_cmd_t *cmd;
    L4VM_server_cmd_params_t *params;

    dprintk( 5, KERN_INFO PREFIX "server thread: write request\n" );

    cmd_idx = L4VMpci_cmd_allocate_bottom();
    
    cmd = &server->bottom_half_cmds.cmds[ cmd_idx ];
    params = &server->bottom_half_params[ cmd_idx ];
    params->r_w.idx = idx;
    params->r_w.reg = reg;
    params->r_w.len = len;
    params->r_w.val = val;
    params->r_w.offset = offset;

    cmd->reply_to_tid = _caller;
    cmd->data = params;
    cmd->handler = L4VMpci_server_write_handler;

    L4VMpci_deliver_irq(L4VMPCI_IRQ_BOTTOM_HALF_CMD);
    idl4_set_no_response( _env );  // Handle the request in a safe context.

    return 0;
}
IDL4_PUBLISH_ATTR
IDL4_PUBLISH_IVMPCI_CONTROL_WRITE(IVMpci_Control_write_implementation);

static void *IVMpci_Control_vtable[IVMPCI_CONTROL_DEFAULT_VTABLE_SIZE] = IVMPCI_CONTROL_DEFAULT_VTABLE;

void IVMpci_Control_server(void)
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long cnt;
    IVMpci_dev_id_t dev_ids[20];

    idl4_msgbuf_init(&msgbuf);
    idl4_msgbuf_add_buffer( &msgbuf, dev_ids, sizeof(dev_ids) );

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

	    idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IVMpci_Control_vtable[idl4_get_function_id(&msgtag) & IVMPCI_CONTROL_FID_MASK]);
	}
    }
}

void IVMpci_Control_discard(void)
{
    L4_KDB_Enter( "received IPC with invalid format" );
}

/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/lanaddress/lanaddress.c
 * Description:	Implements a simple L4 server that distributes virtual
 * 		LAN addresses to other L4 clients.  It derives the virtual
 * 		LAN address from the physical, thus ensuring that
 * 		virtual machines on different hosts have unique virtual
 * 		LAN addresses.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>

#include <glue/wedge.h>
#include <glue/thread.h>
#include <glue/vmserver.h>

#include "lanaddress_idl_server.h"

#define PREFIX "lanaddress: "

/***************************************************************************
 *
 * Data structures.
 *
 ***************************************************************************/

static L4_ThreadId_t lanaddress_tid = L4_nilthread;

static lanaddress_t lanaddress_base;
static L4_Word16_t lanaddress_handle = 0;
static int lanaddress_valid = 0;

extern inline void
lanaddress_print_addr( lanaddress_t *lanaddr )
{
    int i;
    for( i = 0; i < ETH_ALEN; i++ )
    {
	if(i) printk(":");
	printk( "%02x", lanaddr->raw[i] );
    }
}


/***************************************************************************
 *
 * Service functions.
 *
 ***************************************************************************/

IDL4_INLINE void ILanAddress_generate_lan_address_implementation(
	CORBA_Object _caller,
	lanaddress_t *lanaddress,
	idl4_server_environment *_env)
{
    if( !lanaddress_valid )
    {
	CORBA_exception_set( _env, ex_ILanAddress_insufficient_resources, NULL);
	return;
    }

    // Create a new address, combined from the base, and the handle.
    lanaddress_set_handle( &lanaddress_base, lanaddress_handle );
    lanaddress->align4.lsb = lanaddress_base.align4.lsb;
    lanaddress->align4.msb = lanaddress_base.align4.msb;

    // Move to the next handle.
    lanaddress_handle++;
    if( lanaddress_handle == 0 )
    {
	// wrap around
	lanaddress_valid = 0;
    }
}
IDL4_PUBLISH_ILANADDRESS_GENERATE_LAN_ADDRESS(ILanAddress_generate_lan_address_implementation);


void lanaddress_server_thread(void *p)
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long cnt;
    static void *ILanAddress_vtable[ILANADDRESS_DEFAULT_VTABLE_SIZE] = 
	ILANADDRESS_DEFAULT_VTABLE;

    idl4_msgbuf_init( &msgbuf );

    while( 1 )
    {
	partner = L4_nilthread;
	msgtag.raw = 0;
	cnt = 0;

	while( 1 )
	{
	    idl4_msgbuf_sync( &msgbuf );
	    idl4_reply_and_wait( &partner, &msgtag, &msgbuf, &cnt );
	    if( idl4_is_error( &msgtag ) )
	     	break;

	    idl4_process_request( &partner, &msgtag, &msgbuf, &cnt, 
		    ILanAddress_vtable[
		        idl4_get_function_id(&msgtag) & ILANADDRESS_FID_MASK] );
	}
    }
}

/***************************************************************************
 *
 * Module initialization and tear down.
 *
 ***************************************************************************/

static void __init
landaddress_generate_base_address( void )
{
    read_lock( &dev_base_lock );
    {
	struct net_device *dev;
	for( dev = dev_base; dev; dev = dev->next )
	{
	    // See include/linux/if_arp.h for device type constants.  They
	    // supposedly conform to ARP protocol hardware identifiers.
	    if( (dev->type == ARPHRD_ETHER) && (dev->addr_len == ETH_ALEN) )
	    {
		int i;

		// Reverse the hardware address, to make a slightly random
		// LAN address, but one which we will always generate for
		// this particular device/machine.
		for( i = 0; i < ETH_ALEN; i++ )
		    lanaddress_base.raw[i] = dev->dev_addr[ETH_ALEN-1-i];

		lanaddress_set_handle( &lanaddress_base, 0 );
		lanaddress_base.status.local = 1;
		lanaddress_base.status.group = 0;

		lanaddress_valid = 1;
		break;
	    }
	}
    }
    read_unlock( &dev_base_lock );
}

static int __init
lanaddress_init_module( void )
{
    int err;

    landaddress_generate_base_address();

    if( lanaddress_valid )
    {
	printk( KERN_INFO PREFIX "base address: " );
	lanaddress_print_addr( &lanaddress_base );
	printk( "\n" );
    }
    else
	printk( KERN_ERR PREFIX "unable to generate a base address.\n" );

    lanaddress_tid = L4VM_thread_create( GFP_KERNEL, lanaddress_server_thread,
	    l4ka_wedge_get_irq_prio(), smp_processor_id(), NULL, 0 );

    err = L4VM_server_register_location( UUID_ILanAddress, lanaddress_tid );
    if( err == -ENODEV )
	printk( KERN_ERR PREFIX "failed to register with the locator.\n" );
    else if (err < 0 )
	printk( KERN_ERR PREFIX "failed to contact the locator service.\n" );

    printk( KERN_INFO PREFIX "initialized.\n" );

    return 0;
}

static void __exit
lanaddress_exit_module( void )
{
    if( !L4_IsNilThread(lanaddress_tid) )
    {
	L4VM_server_register_location( UUID_ILanAddress, L4_nilthread );
	L4VM_thread_delete( lanaddress_tid );
	lanaddress_tid = L4_nilthread;
    }
}

module_init( lanaddress_init_module );
module_exit( lanaddress_exit_module );

MODULE_AUTHOR( "Joshua LeVasseur <jtl@ira.uka.de>" );
MODULE_DESCRIPTION( "LAN address generator" );
MODULE_LICENSE( "Dual BSD/GPL" );
MODULE_VERSION( "yoda" );

#if defined(MODULE)
// Define a .afterburn_wedge_module section, to activate sybmol resolution for
// this module against the wedge's symbols.
__attribute__ ((section(".afterburn_wedge_module")))
burn_wedge_header_t burn_wedge_header = {
    BURN_WEDGE_MODULE_INTERFACE_VERSION
};
#endif


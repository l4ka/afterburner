/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/net/net.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include "L4VMnet_idl_client.h"
#include "net.h"
#include "lanaddress_idl_client.h"

#if !defined(CONFIG_DRIVERS_NET_OPTIMIZE)
int L4VMnet_debug_level = 4;
MODULE_PARM( L4VMnet_debug_level, "i" );
#endif

int L4VMnet_allocate_lan_address( char lanaddress[ETH_ALEN] )
{
    L4_ThreadId_t lanaddress_tid;
    CORBA_Environment ipc_env;
    lanaddress_t reply_addr;
    unsigned long irq_flags;
    int err;

    err = L4VM_server_locate( UUID_ILanAddress, &lanaddress_tid );
    if( err != 0 )
	return err;

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    ILanAddress_generate_lan_address( lanaddress_tid, &reply_addr, &ipc_env );
    local_irq_restore(irq_flags);
    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -ENODEV;
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -EIO; 
    }

    memcpy( lanaddress, reply_addr.raw, ETH_ALEN );
    return 0;
}

void L4VMnet_print_lan_address( unsigned char *lanaddr )
{
    int i;
    for( i = 0; i < ETH_ALEN; i++ )
    {
	if(i) printk(":");
	printk( "%02x", lanaddr[i] );
    }
}


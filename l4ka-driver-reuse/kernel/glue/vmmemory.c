/*********************************************************************
 *
 * Copyright (C) 2004-2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/vmmemory.c
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

#include <l4/types.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>

#include <asm/io.h>

#include "wedge.h"
#include "vmmemory.h"
#include "resourcemon_idl_client.h"

EXPORT_SYMBOL(L4VM_fpage_alloc);
EXPORT_SYMBOL(L4VM_fpage_vmarea_get);
EXPORT_SYMBOL(L4VM_fpage_vmarea_release);
EXPORT_SYMBOL(L4VM_tid_to_space_id);
EXPORT_SYMBOL(L4VM_get_client_space_info);
EXPORT_SYMBOL(L4VM_get_space_info);
EXPORT_SYMBOL(L4VM_get_client_dma_info);
EXPORT_SYMBOL(L4VM_get_space_dma_info);
EXPORT_SYMBOL(L4VM_page_in);


int
L4VM_fpage_alloc( L4_Word_t req_log2size, L4VM_alligned_alloc_t *info )
{
    L4_Word_t order;
    
    if( req_log2size < PAGE_SHIFT )
	req_log2size = PAGE_SHIFT;
    order = req_log2size - PAGE_SHIFT;

    info->actual_log2size = req_log2size;
    info->start = __get_free_pages( GFP_KERNEL, order );
    if( info->start == 0 )
	return -ENOMEM;

    info->fpage = L4_FpageLog2( info->start, req_log2size );
    if( L4_Address(info->fpage) == info->start )
	return 0;

    // Double the allocation size, so that we can align it.
    free_pages( info->start, order );
    order += 1;
    info->actual_log2size += 1;
    info->start = __get_free_pages( GFP_KERNEL, order );
    if( info->start == 0 )
	return -ENOMEM;

    info->fpage = L4_FpageLog2( info->start + (1 << req_log2size), 
	    req_log2size );
    return 0;
}

int
L4VM_fpage_vmarea_get( L4_Word_t req_log2size, L4VM_alligned_vmarea_t *info )
{
    L4_Word_t actual_log2size, size;

    if( req_log2size < PAGE_SHIFT )
	req_log2size = PAGE_SHIFT;
    actual_log2size = req_log2size;
    size = 1 << actual_log2size;

    // We'd really like to call get_vm_area(), but it isn't exposed to
    // kernel modules.
    info->ioremap_addr = ioremap( 0 - size, size );
    if( info->ioremap_addr == NULL )
	return -ENOMEM;

    // Try to create the fpage.
    info->fpage = L4_FpageLog2( (L4_Word_t)info->ioremap_addr, req_log2size );
    if( L4_Address(info->fpage) == (L4_Word_t)info->ioremap_addr )
	return 0;

    // Realign somewhere within the oversized region.
    L4VM_fpage_vmarea_release( info );
    actual_log2size++; // Double the size, so that we can align.
    size = 1 << actual_log2size;

    info->ioremap_addr = ioremap( 0 - size, size );
    if( info->ioremap_addr == NULL )
	return -ENOMEM;

    info->fpage = L4_FpageLog2( 
	    (L4_Word_t)info->ioremap_addr + (1 << req_log2size),
	    req_log2size );
    return 0;
}

void L4VM_fpage_vmarea_release( L4VM_alligned_vmarea_t *info )
{
    if( info->ioremap_addr )
	iounmap( info->ioremap_addr );
    info->ioremap_addr = NULL;
}

int L4VM_tid_to_space_id( L4_ThreadId_t tid, L4_Word_t *space_id )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
               
    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IResourcemon_tid_to_space_id( resourcemon_shared.resourcemon_tid,  &tid, space_id, &ipc_env );
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

    return 0;
}


static L4VM_client_space_info_t *client_list = NULL;
static spinlock_t client_space_lock = SPIN_LOCK_UNLOCKED;

L4VM_client_space_info_t * L4VM_get_client_space_info( L4_ThreadId_t tid )
{
    L4_Word_t space_id;

    if( L4VM_tid_to_space_id(tid, &space_id) )
	return NULL;

    return L4VM_get_space_info( space_id );
}

L4VM_client_space_info_t * L4VM_get_space_info( L4_Word_t space_id )
{
    /* This function mantains a central list of ioremaps for each
     * client's address space.
     */

    L4VM_client_space_info_t *c;
    L4_Word_t bus_start, bus_size;

    // Search for a preexisting ioremap for this particular client.
    spin_lock( &client_space_lock );
    for( c = client_list; c; c = c->next )
	if( c->space_id == space_id )
	    break;
    if( c )
    {
	c->refcnt++;
	spin_unlock( &client_space_lock );
	return c;
    }

    // Get info about the client's machine address space.
    if( L4VM_get_space_dma_info(space_id, &bus_start, &bus_size) )
    {
	spin_unlock( &client_space_lock );
	return NULL;
    }


    // Safe to proceed, so allocate a new structure and add to the list.
    c = (L4VM_client_space_info_t *)
	kmalloc( sizeof(L4VM_client_space_info_t), GFP_KERNEL );
    if( c == NULL )
    {
	spin_unlock( &client_space_lock );
	return NULL;
    }

    c->space_id = space_id;
    c->bus_start = bus_start;
    c->bus_size = bus_size;
    c->refcnt = 1;
    c->next = client_list;
    client_list = c;

    spin_unlock( &client_space_lock );

    return c;
}


int L4VM_get_client_dma_info(
	L4_ThreadId_t client_tid, 
	L4_Word_t *phys_start,
	L4_Word_t *phys_size )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IResourcemon_get_client_phys_range( 
	    resourcemon_shared.resourcemon_tid, &client_tid, phys_start, phys_size, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free( &ipc_env );
	return -ENODEV;
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &ipc_env );
	return -EIO;
    }

    return 0;
}

int L4VM_get_space_dma_info(
	L4_Word_t space_id,
	L4_Word_t *phys_start,
	L4_Word_t *phys_size )
{
    CORBA_Environment ipc_env;
    unsigned long irq_flags;

    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IResourcemon_get_space_phys_range( 
	resourcemon_shared.resourcemon_tid, space_id, phys_start, phys_size, &ipc_env );
    local_irq_restore(irq_flags);

    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free( &ipc_env );
	return -ENODEV;
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &ipc_env );
	return -EIO;
    }

    return 0;
}


void L4VM_page_in( L4_Fpage_t fp, L4_Word_t page_size )
{
    volatile L4_Word_t *page = (volatile L4_Word_t *)L4_Address(fp);
    L4_Word_t *end = (L4_Word_t *)(L4_Address(fp) + L4_Size(fp));

    while( page < end ) {
	*page;
	page += page_size;
    }
}


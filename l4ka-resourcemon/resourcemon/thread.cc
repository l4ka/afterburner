/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:	resourcemon/thread.cc
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

#include <l4/thread.h>
#include <l4/message.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <l4/kdebug.h>

#include <common/debug.h>
#include <resourcemon/resourcemon.h>
#include "resourcemon_idl_server.h"
#include <resourcemon/vm.h>

#if defined(cfg_l4ka_vmextensions)
#include <resourcemon/virq.h>
#endif
#if defined(cfg_logging)
#include <resourcemon/logging.h>
#endif
static vm_t *irq_to_vm[MAX_IRQS];


extern inline bool is_valid_client_thread( L4_ThreadId_t tid )
{
    return NULL != get_vm_allocator()->tid_to_vm(tid);
}


IDL4_INLINE int IResourcemon_ThreadControl_implementation(
	CORBA_Object _caller,
	const L4_ThreadId_t *dest,
	const L4_ThreadId_t *space,
	const L4_ThreadId_t *sched,
	const L4_ThreadId_t *pager,
	const L4_Word_t utcb_location,
	const L4_Word_t prio,
	idl4_server_environment *_env)
{
    if( !is_valid_client_thread(_caller) )
    {
	// Don't respond to invalid clients.
	idl4_set_no_response( _env );
	printf( PREFIX "unknown client %p\n", RAW(_caller) );
	return 0;
    }
  
    int result = L4_ThreadControl( *dest, *space, *sched, *pager, (void*)utcb_location);
   
    if( !result )
    {
	printf( "Creating thread tid %t failed with error %d\n", *dest, L4_ErrorCode());
	CORBA_exception_set( _env, L4_ErrorCode() + ex_IResourcemon_ErrOk, NULL );
	return result;
    }
    
#if defined(cfg_l4ka_vmextensions) || defined(cfg_logging)
    L4_Word_t prio_control = 0;
    L4_Word_t dummy;
	    
#if defined(cfg_l4ka_vmextensions)
    if (prio != 0 && prio <= get_vm_allocator()->tid_to_vm(_caller)->get_prio())
	prio_control = prio;
#endif
#if defined(cfg_logging)
    L4_Word_t domain = get_vm_allocator()->tid_to_vm(_caller)->get_space_id() + VM_DOMAIN_OFFSET;
    prio_control |= domain << 16;
#endif
    
    if (!L4_Schedule(*dest, ~0UL, ~0UL, prio_control, ~0UL, &dummy))
    {
	printf( "Error: unable to set a thread's prio_control to %x, L4 error: ", prio_control, L4_ErrorCode());
	return NULL;
    }
#endif
    
    return result;
}
IDL4_PUBLISH_IRESOURCEMON_THREADCONTROL(IResourcemon_ThreadControl_implementation);


IDL4_INLINE int IResourcemon_SpaceControl_implementation(
	CORBA_Object _caller,
	const L4_ThreadId_t *space,
	const L4_Word_t control,
	const L4_Word_t kip,
	const L4_Word_t utcb,
	const L4_ThreadId_t *redir,
	idl4_server_environment *_env)
{
    if( !is_valid_client_thread(_caller) )
    {
	// Don't respond to invalid clients.
	idl4_set_no_response( _env );
	printf( PREFIX "unknown client %p\n", RAW(_caller) );
	return 0;
    }

    L4_Fpage_t kipfp( (L4_Fpage_t){raw: kip} );
    L4_Fpage_t utcbfp( (L4_Fpage_t){raw: utcb} );
    L4_Word_t dummy;

    int result = L4_SpaceControl( *space, control, kipfp, utcbfp, *redir, &dummy);
    
    if( !result )
	CORBA_exception_set( _env, 
    		L4_ErrorCode() + ex_IResourcemon_ErrOk, NULL );
    return result;
}
IDL4_PUBLISH_IRESOURCEMON_SPACECONTROL(IResourcemon_SpaceControl_implementation);


IDL4_INLINE int IResourcemon_AssociateInterrupt_implementation(
	CORBA_Object _caller,
	const L4_ThreadId_t *irq_tid,
	const L4_ThreadId_t *handler_tid,
	const L4_Word_t irq_prio,
	const L4_Word_t irq_cpu,
	idl4_server_environment *_env)
{
    vm_t *vm;
    int irq = irq_tid->global.X.thread_no;
    
    
    if( (vm = get_vm_allocator()->tid_to_vm(_caller)) == NULL)
    {
	// Don't respond to invalid clients.
	idl4_set_no_response( _env );
	printf( PREFIX "unknown client %p\n", RAW(_caller) );
	return 0;
    }
    if (irq >= MAX_IRQS){
	printf( PREFIX "IRQ %d >= MAX_IRQS\n", irq);
	return 0;
    }
    
#if defined(cfg_l4ka_vmextensions)
    return associate_virtual_interrupt(vm, *irq_tid, *handler_tid, irq_cpu);
#endif
    
    if (irq_to_vm[irq] == NULL || irq_to_vm[irq] == vm)
    {
	   
	L4_ThreadId_t real_irq_tid = *irq_tid;
	real_irq_tid.global.X.version = 1;
	
	int result = L4_AssociateInterrupt( real_irq_tid, *handler_tid );
	if( !result )
	{
	    CORBA_exception_set( _env, 
		L4_ErrorCode() + ex_IResourcemon_ErrOk, NULL );
	}
	else 
	{
	    L4_Word_t dummy;
	    
	    if ((irq_prio != 255 || irq_cpu != L4_ProcessorNo()) &&
		!L4_Schedule(real_irq_tid, ~0UL, irq_cpu, irq_prio, ~0UL, &dummy))
		CORBA_exception_set( _env, L4_ErrorCode() + ex_IResourcemon_ErrOk, NULL );
	    else
		irq_to_vm[irq] = vm;
	}
	return result;
	
    } else 
    {
	printf( PREFIX "IRQ %d already associated\n", irq);
	return 0;
    }	
    
}
IDL4_PUBLISH_IRESOURCEMON_ASSOCIATEINTERRUPT(IResourcemon_AssociateInterrupt_implementation);


IDL4_INLINE int IResourcemon_DeassociateInterrupt_implementation(
    CORBA_Object _caller,
    const L4_ThreadId_t *irq_tid,
    idl4_server_environment *_env)
{
    vm_t *vm;
    int irq = irq_tid->global.X.thread_no;

    if( (vm = get_vm_allocator()->tid_to_vm(_caller)) == NULL)
    {
	// Don't respond to invalid clients.
	idl4_set_no_response( _env );
	printf( PREFIX "unknown client %p\n", RAW(_caller) );
	return 0;
    }
    
#if defined(cfg_l4ka_vmextensions)
    printf(PREFIX "Deassociating virtual timer interrupt %u \n", irq);
    if (deassociate_virtual_interrupt(vm, *irq_tid, _caller))
	return 1;
    else return 0;
#endif

    if (irq_to_vm[irq] == vm){
	
	irq_to_vm[irq] = NULL;	
	L4_ThreadId_t real_irq_tid = *irq_tid;
	real_irq_tid.global.X.version = 1;
    
	int result = L4_DeassociateInterrupt( real_irq_tid );
	if( !result )
	    CORBA_exception_set( _env, 
				 L4_ErrorCode() + ex_IResourcemon_ErrOk, NULL );
	return result;
    }
    return 0;
}
IDL4_PUBLISH_IRESOURCEMON_DEASSOCIATEINTERRUPT(IResourcemon_DeassociateInterrupt_implementation);


IDL4_INLINE void IResourcemon_tid_to_space_id_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const L4_ThreadId_t *tid,
	L4_Word_t *space_id,
	idl4_server_environment *_env)
{
    vm_t *vm = get_vm_allocator()->tid_to_vm(*tid);
    if( vm == NULL )
	CORBA_exception_set( _env, ex_IResourcemon_unknown_client, NULL );
    else
	*space_id = vm->get_space_id();
}
IDL4_PUBLISH_IRESOURCEMON_TID_TO_SPACE_ID(IResourcemon_tid_to_space_id_implementation);



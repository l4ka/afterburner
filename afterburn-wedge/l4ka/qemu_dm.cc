/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/ide.cc
 * Description:   Front-end emulation for IDE(ATA) disk/controller
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


#include INC_ARCH(types.h)
#include <debug.h>
#include <l4ka/l4thread.h>
#include <console.h>

#include INC_ARCH(page.h)
#include <debug.h>
#include <l4ka/resourcemon.h>
#include <l4ka/vcpu.h>
#include <l4ka/backend.h>
#include <string.h>

#include "l4ka/qemu_dm.h"

qemu_dm_t *ptr_qemu_dm;

static void qemu_dm_pager_thread(void* params, l4thread_t *thread)
{
    qemu_dm_t *pl = (qemu_dm_t*)params;
    pl->pager_server_loop();
    DEBUGGER_ENTER("Qemu-dm pager loop returned!");
}

/* Interface IQEMU_DM_PAGER::Control */

IDL4_INLINE void  IQEMU_DM_PAGER_Control_request_page_implementation(CORBA_Object  _caller, const IQEMU_DM_PAGER_addr_t  address, idl4_fpage_t * page, idl4_server_environment * _env)

{
    /*
     * address has to be a guest physical address. We have to break up accesses, that span multiple pages, in portio.cc or 
     * in qemu_mmio.cc (handle_mmio())
     */

    dprintf(debug_qemu, "QEMU_DM_PAGER: map Guest physical address = %lx \n",address);

    // Ensure that we have the page.
    *(volatile char *)address;

    L4_Fpage_t fpage = L4_Fpage(address, PAGE_SIZE);
    idl4_fpage_set_page(page,fpage);
    idl4_fpage_set_base(page,address);
    idl4_fpage_set_permissions(page, L4_FullyAccessible);
  
    return;
}

IDL4_PUBLISH_IQEMU_DM_PAGER_CONTROL_REQUEST_PAGE(IQEMU_DM_PAGER_Control_request_page_implementation);

IDL4_INLINE void  IQEMU_DM_PAGER_Control_request_special_page_implementation(CORBA_Object  _caller, const L4_Word_t  index, idl4_fpage_t * page, idl4_server_environment * _env)
{
    L4_Fpage_t fpage;
    L4_Word_t start_addr = 0UL -1;

    switch(index)
    {
	case IQEMU_DM_PAGER_REQUEST_SHARED_IO_PAGE:
	{
	    start_addr = (L4_Word_t)&ptr_qemu_dm->s_pages.sio_page;
	    break;
	}
	case IQEMU_DM_PAGER_REQUEST_BUFFERED_IO_PAGE:
	{
	    start_addr = (L4_Word_t)&ptr_qemu_dm->s_pages.bio_page;
	    break;
	}
	default:
	    start_addr = 0;
    }
    printf("Request special page with index %lu. startaddr = %lx\n",index,start_addr);
    if(start_addr == (0UL -1))
    {
	CORBA_exception_set(_env, ex_IQEMU_DM_PAGER_invalid_index, NULL);
	return;
    }
    
    fpage = L4_Fpage(start_addr, PAGE_SIZE);
    idl4_fpage_set_page(page,fpage);
    idl4_fpage_set_base(page,0);
    idl4_fpage_set_permissions(page, L4_FullyAccessible);
     
    return;
}

IDL4_PUBLISH_IQEMU_DM_PAGER_CONTROL_REQUEST_SPECIAL_PAGE(IQEMU_DM_PAGER_Control_request_special_page_implementation);

IDL4_INLINE void  IQEMU_DM_PAGER_Control_raise_irq_implementation(CORBA_Object  _caller, const L4_Word_t irqmask, idl4_server_environment * _env)

{
    
    for(int i = 0; i <= 15;i++)
	if(irqmask & (1 << i))
	    get_intlogic().raise_irq(i);
  
    return;
}

IDL4_PUBLISH_IQEMU_DM_PAGER_CONTROL_RAISE_IRQ(IQEMU_DM_PAGER_Control_raise_irq_implementation);

void  IQEMU_DM_PAGER_Control_discard()

{
}

void * IQEMU_DM_PAGER_Control_vtable[IQEMU_DM_PAGER_CONTROL_DEFAULT_VTABLE_SIZE] = IQEMU_DM_PAGER_CONTROL_DEFAULT_VTABLE;

void  qemu_dm_t::pager_server_loop(void)
{
  L4_ThreadId_t  partner;
  L4_MsgTag_t  msgtag;
  idl4_msgbuf_t  msgbuf;
  long  cnt;

  idl4_msgbuf_init(&msgbuf);

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

          idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IQEMU_DM_PAGER_Control_vtable[idl4_get_function_id(&msgtag) & IQEMU_DM_PAGER_CONTROL_FID_MASK]);
        }
    }

}

L4_Word_t qemu_dm_t::raise_event(L4_Word_t event)
{
    CORBA_Environment ipc_env;
    ipc_env = idl4_default_environment;
    
    IQEMU_DM_Control_raiseEvent(qemu_dm_server_id, event , &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "qemu-dm backend: raising event failed\n");
	return 1;
    }
    return 0;
}

void qemu_dm_t::init(void)
{

    printf("Initialize Qemu backend\n");
    ptr_qemu_dm = this;

    CORBA_Environment ipc_env;
    ipc_env = idl4_default_environment;
    
    // locate Qemu-dm server
    IResourcemon_query_interface( L4_Pager(), UUID_IQEMU_DM, &qemu_dm_server_id, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "unable to locate block server\n");
	return;
    }

    init_shared_pages();
    dprintf(debug_qemu, "shared_iopage at addr %p, buffered_iopage at addr %p\n", &s_pages.sio_page, &s_pages.bio_page);

     // start pager loop thread
    vcpu_t &vcpu = get_vcpu();
    l4thread_t *pager_thread = get_l4thread_manager()->create_thread( &vcpu, (L4_Word_t)qemu_dm_pager_stack,
								    sizeof(qemu_dm_pager_stack), 
								    resourcemon_shared.prio, 
								    qemu_dm_pager_thread, 
								    L4_Pager(), 
								    this );
    if( !pager_thread ) {
	printf( "Error creating qemu-dm pager thread\n");
	DEBUGGER_ENTER("qemu-dm pager thread");
	return;
    }

    pager_id.raw = pager_thread->get_global_tid().raw;
    pager_thread->start();

    dprintf(debug_qemu, "Qemu Pager loop started with thread id %t\n", pager_id.raw);

    ipc_env = idl4_default_environment;

    IQEMU_DM_Control_register(qemu_dm_server_id, pager_id.raw, irq_server_id.raw ,&ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "QEMU_DM registration failed\n");
	return;
    }

    dprintf(debug_qemu, "Qemu backend initialization done\n");
}

L4_Word_t qemu_dm_t::send_pio(L4_Word_t port, L4_Word_t count, L4_Word_t size,
	       L4_Word_t &value, uint8_t dir, uint8_t df, uint8_t value_is_ptr)
{
    ioreq_t *p;

    if ( size == 0 || count == 0 ) {
        printf("qemu-dm backend: null pio request? port %lx, count %lx, size %d, value %lx, dir %d, value_is_ptr %d.\n",
               port, count, size, value, dir, value_is_ptr);
    }

    p = get_ioreq_page();
    if ( p->state != STATE_IOREQ_NONE )
        printf("qemu-dm backend: WARNING: send pio with something already pending (%d)?\n",
               p->state);

    p->dir = dir;
    p->data_is_ptr = value_is_ptr;

    p->type = IOREQ_TYPE_PIO;
    p->size = size;
    p->addr = port;
    p->count = count;
    p->df = df;

    p->io_count++;

    p->data = value;

    wmb(); //first set values than raise event

    p->state = STATE_IOREQ_READY;
    
//    dprintf(debug_qemu, "Qemu-dm backend: Raise event  port %lx, count %lx, size %d, value %lx, dir %d, value_is_ptr %d.\n",
    //      port, count, size, value, dir, value_is_ptr);

    if(raise_event(IQEMU_DM_EVENT_IO_REQUEST))
    {
	p->state = STATE_IOREQ_NONE;
	printf("Qemu-dm backend: raise_event failed\n");
	return 0;
    }

    if(dir == IOREQ_READ)
    {
	if(size == 1)
	    value = p->data & 0xff ;
	else if(size == 2)
	    value = p->data & 0xffff;
	else
	    value = p->data;
	
    }
    wmb(); //first write value back than set req state to nonee

    p->state = STATE_IOREQ_NONE;
    return 1;


}

#if defined(CONFIG_L4KA_HVM)

L4_Word_t qemu_dm_t::send_mmio_req(uint8_t type, L4_Word_t gpa,
		       L4_Word_t count, L4_Word_t size, L4_Word_t value,
		       uint8_t dir, uint8_t df, uint8_t value_is_ptr)
{
    ioreq_t *p;

    if ( size == 0 || count == 0 ) {
        dprintf(debug_qemu,"qemu-dm backend: null mmio request? type %lx, gda %lx, count %lx, size %d, value %lx, dir %d, value_is_ptr %d.\n",
		type, gpa, count, size, value, dir, value_is_ptr);
    }

    p = get_ioreq_page();

    if ( p->state != STATE_IOREQ_NONE )
        dprintf(debug_qemu,"WARNING: send mmio with something already pending (%d)?\n",
               p->state);

    p->dir = dir;
    p->data_is_ptr = value_is_ptr;

    p->type = type;
    p->size = size;
    p->addr = gpa;
    p->count = count;
    p->df = df;

    p->io_count++;

    p->data = value;

    wmb(); //first set values than raise event

    p->state = STATE_IOREQ_READY;

    //printf("mmio req t %x, gpa %x, dir %x\n", type, gpa, dir);

    /*printf("mmio: type %lx, gda %lx, count %lx, size %d, value %lx, dir %d, value_is_ptr %d\n",
      type, gpa, count, size, value, dir, value_is_ptr);*/


    if(raise_event(IQEMU_DM_EVENT_IO_REQUEST))
    {
	p->state = STATE_IOREQ_NONE;
	printf("Qemu-dm backend: raise_event failed\n");
	return 0;
    }

    extern void hvm_mmio_assist(void);
    hvm_mmio_assist();

    p->state = STATE_IOREQ_NONE;
    return 1;

}

#else

L4_Word_t qemu_dm_t::send_mmio_req(uint8_t type, L4_Word_t gpa,
		       L4_Word_t count, L4_Word_t size, L4_Word_t value,
		       uint8_t dir, uint8_t df, uint8_t value_is_ptr)
{ return 0; }
#endif

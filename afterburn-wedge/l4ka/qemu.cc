/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/qemu.cc
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
#include <l4ka/qemu.h>
#include "device/rtc.h"

qemu_t *ptr_l4ka_driver_reuse_qemu;

typedef struct guest_mem_entry {
    L4_Word_t start;
    L4_Word_t size;
    L4_Word_t type;
} guest_mem_entry_t;

#define MAX_GUEST_MEM_ENTRIES 10
L4_Word_t guest_mem_num_entry = 0;
guest_mem_entry_t guest_mem_entries[MAX_GUEST_MEM_ENTRIES];

static void register_guest_mem(L4_Word_t start, L4_Word_t size, L4_Word_t type)
{
    uint32_t i;
    for(i = 0; i < guest_mem_num_entry; i++)
    {
	if(guest_mem_entries[i].start == start)
	{
	    if(guest_mem_entries[i].type == IQemu_pager_vmem && type != IQemu_pager_invalid_type)
	    {
		printf("Do not override video ram area. start %lx, size %lx, type %d\n",start, size,type);
		goto debug_out;
	    }
	    guest_mem_entries[i].size = size;
	    guest_mem_entries[i].type = type;
	    goto debug_out;
	}
	else if(guest_mem_entries[i].type == IQemu_pager_invalid_type)
	{
	    //entry scheduled for deletion. overwrite now
	    guest_mem_entries[i].start = start;
	    guest_mem_entries[i].size = size;
	    guest_mem_entries[i].type = type;
	    goto debug_out;
	}
//debug
	else if( (start > guest_mem_entries[i].start && start < guest_mem_entries[i].start + guest_mem_entries[i].size) ||
		 (start + size > guest_mem_entries[i].start && start + size  < guest_mem_entries[i].start + guest_mem_entries[i].size) ||
		 (start < guest_mem_entries[i].start && start + size  > guest_mem_entries[i].start) )
	{
	    printf("memory area overlaps. new entry: start %lx, size %lx, type %d, current entry: start %lx, size %lx, type %d\n",
		   start,size,type, guest_mem_entries[i].start, guest_mem_entries[i].size, guest_mem_entries[i].type);
	    DEBUGGER_ENTER("UNTESTED");
	}
    }

    if(i == MAX_GUEST_MEM_ENTRIES)
    {
	printf("Out of guest mem entries\n");
	DEBUGGER_ENTER("UNTESTED");
    }
    guest_mem_entries[guest_mem_num_entry].start = start;
    guest_mem_entries[guest_mem_num_entry].size = size;
    guest_mem_entries[guest_mem_num_entry++].type = type;

debug_out:
    //debug print mem area table
    for(i = 0; i < guest_mem_num_entry;i++)
	printf("start %lx, size %lx, type %d\n",guest_mem_entries[i].start, guest_mem_entries[i].size, guest_mem_entries[i].type);
}

static guest_mem_entry_t *get_guest_mem_entry( L4_Word_t addr)
{
    for(uint32_t i = 0; i < guest_mem_num_entry; i++)
    {
	if(addr >= guest_mem_entries[i].start && addr < guest_mem_entries[i].start + guest_mem_entries[i].size)
	    return &guest_mem_entries[i];
    }
    return NULL;
}

static void qemu_pager_thread(void* params, l4thread_t *thread)
{
    qemu_t *pl = (qemu_t*)params;
    pl->pager_server_loop();
    DEBUGGER_ENTER("Qemu-dm pager loop returned!");
}

/* Interface IQEMU_PAGER::Control */

IDL4_INLINE void  IQemu_pager_Control_request_page_implementation(CORBA_Object  _caller, const IQemu_pager_addr_t  address, idl4_fpage_t * page, idl4_server_environment * _env)

{
    /*
     * address has to be a guest physical address. We have to break up accesses, that span multiple pages, in portio.cc or 
     * in qemu_mmio.cc (handle_mmio())
     */

    dprintf(debug_qemu, "L4KA_DRIVER_REUSE_QEMU_PAGER: map Guest physical address = %lx \n",address);

    // Ensure that we have the page.
    *(volatile char *)address;

    L4_Fpage_t fpage = L4_Fpage(address, PAGE_SIZE);
    idl4_fpage_set_page(page,fpage);
    idl4_fpage_set_base(page,address);
    idl4_fpage_set_permissions(page, L4_FullyAccessible);
  
    return;
}

IDL4_PUBLISH_IQEMU_PAGER_CONTROL_REQUEST_PAGE(IQemu_pager_Control_request_page_implementation);

IDL4_INLINE void  IQemu_pager_Control_request_special_page_implementation(CORBA_Object  _caller, const L4_Word_t  index, const L4_Word_t offset, idl4_fpage_t * page, idl4_server_environment * _env)
{
    L4_Fpage_t fpage;
    L4_Word_t start_addr = 0UL -1;

    switch(index)
    {
	case IQemu_pager_shared_io_page:
	{
	    start_addr = (L4_Word_t)&ptr_l4ka_driver_reuse_qemu->s_pages.sio_page;
	    break;
	}
	case IQemu_pager_vmem:
	{
	    start_addr = ((L4_Word_t)&ptr_l4ka_driver_reuse_qemu->video_mem + offset) & PAGE_MASK;
	    break;
	}
    }
    if(start_addr == (0UL -1))
    {
	CORBA_exception_set(_env, ex_IQemu_pager_invalid_index, NULL);
	return;
    }
    
    fpage = L4_Fpage(start_addr, PAGE_SIZE);
    idl4_fpage_set_page(page,fpage);
    idl4_fpage_set_base(page,0);
    idl4_fpage_set_permissions(page, L4_FullyAccessible);
     
    return;
}

IDL4_PUBLISH_IQEMU_PAGER_CONTROL_REQUEST_SPECIAL_PAGE(IQemu_pager_Control_request_special_page_implementation);

IDL4_INLINE void  IQemu_pager_Control_set_guest_memory_mapping_implementation(CORBA_Object  _caller, const L4_Word_t  start, const L4_Word_t  size, const L4_Word_t  type, idl4_server_environment * _env)

{

    printf("register guest memory area: start %x, size %x, type %d\n",start,size,type);
    register_guest_mem(start, size, type);
    return;
}

IDL4_PUBLISH_IQEMU_PAGER_CONTROL_SET_GUEST_MEMORY_MAPPING(IQemu_pager_Control_set_guest_memory_mapping_implementation);

IDL4_INLINE void  IQemu_pager_Control_raise_irq_implementation(CORBA_Object  _caller, const L4_Word_t irqmask, idl4_server_environment * _env)
{
    for(int i = 0; i <= 15;i++)
	if(irqmask & (1 << i))
	    get_intlogic().raise_irq(i);
}

IDL4_PUBLISH_IQEMU_PAGER_CONTROL_RAISE_IRQ(IQemu_pager_Control_raise_irq_implementation);

IDL4_INLINE void  IQemu_pager_Control_setCMOSData_implementation(CORBA_Object  _caller, const L4_Word_t  port_addr, const L4_Word_t  data, idl4_server_environment * _env)

{
    /* we do not allow writes to the clock data registers (00h-09h) 
       and to memory information registers (15h-18h,30h,31h,34h,35h)
       all setup by monitor during startup */
    switch(port_addr)
    {
    case 0x00 ... 0x09:
    case 0x15 ... 0x18:
    case 0x30 ... 0x31:
    case 0x34 ... 0x35:
	break;
    default:
	if(port_addr >= 10 && port_addr <= 128 && port_addr)
	    rtc_set_cmos_data(port_addr, data);
    }

}

IDL4_PUBLISH_IQEMU_PAGER_CONTROL_SETCMOSDATA(IQemu_pager_Control_setCMOSData_implementation);

void  IQemu_pager_Control_discard()

{
}

void * IQemu_pager_Control_vtable[IQEMU_PAGER_CONTROL_DEFAULT_VTABLE_SIZE] = IQEMU_PAGER_CONTROL_DEFAULT_VTABLE;

void  qemu_t::pager_server_loop(void)
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

          idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IQemu_pager_Control_vtable[idl4_get_function_id(&msgtag) & IQEMU_PAGER_CONTROL_FID_MASK]);
        }
    }

}

L4_Word_t qemu_t::raise_event(L4_Word_t event)
{
    CORBA_Environment ipc_env;
    ipc_env = idl4_default_environment;
    
    IQemu_Control_raiseEvent(qemu_server_id, event , &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "qemu-dm backend: raising event failed\n");
	return 1;
    }
    return 0;
}

bool qemu_t::handle_pfault(map_info_t &map_info, word_t paddr,bool &nilmapping)
{
    guest_mem_entry_t *entry;
    word_t offset;
    entry = get_guest_mem_entry(paddr);
    if(entry == NULL)
    {
	//no guest mem entry for this addr
	return false;
    }
    
    if(paddr >= 0xA0000 && paddr < 0xC0000) //ignore framebuffer
	return false;

    //  printf("entry type = %d\n",entry->type);
    switch(entry->type)
    {
	case IQemu_pager_mmio:
	    extern void handle_mmio(word_t gpa); 
	    handle_mmio(paddr);
	    nilmapping = true;
	    return true;

	case IQemu_pager_vmem:
	    offset = paddr - entry->start;
	    map_info.addr = ((word_t)video_mem + offset) & PAGE_MASK;
	    map_info.page_bits = DEFAULT_PAGE_BITS;
	    map_info.rwx = 7;
	    printf("map vmem page. addr %lx \n",map_info.addr);
	    return true;
	default:
	    DEBUGGER_ENTER("INVALID QEMU mem area type");
    }
    return false;
}

void qemu_t::init(void)
{

    printf("Initialize Qemu backend\n");
    ptr_l4ka_driver_reuse_qemu = this;

    CORBA_Environment ipc_env;
    ipc_env = idl4_default_environment;
    
    // locate Qemu-dm server 
    IResourcemon_query_interface( L4_Pager(), UUID_IQemu, &qemu_server_id, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "unable to locate block server\n");
	return;
    }

    init_shared_pages();

#ifdef CONFIG_L4KA_DRIVER_REUSE_QEMU_WITH_PIC
    init_cmos();
    irq_is_pending = false;
#endif
    dprintf(debug_qemu, "shared_iopage at addr %p, buffered_iopage at addr %p\n", &s_pages.sio_page);

     // start pager loop thread
    vcpu_t &vcpu = get_vcpu();
    l4thread_t *pager_thread = get_l4thread_manager()->create_thread( &vcpu, (L4_Word_t)qemu_pager_stack,
								    sizeof(qemu_pager_stack), 
								    resourcemon_shared.prio, 
								    qemu_pager_thread, 
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

    IQemu_Control_register(qemu_server_id, pager_id.raw, irq_server_id.raw ,&ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "L4KA_DRIVER_REUSE_QEMU registration failed\n");
	return;
    }

    dprintf(debug_qemu, "Qemu backend initialization done\n");
}

L4_Word_t qemu_t::send_pio(L4_Word_t port, L4_Word_t count, L4_Word_t size,
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

//    printf("qemu-dm backend: pio request port %lx, count %lx, size %d, value %lx, dir %d, value_is_ptr %d.\n",
//               port, count, size, value, dir, value_is_ptr);

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
    
    if(raise_event(IQemu_event_io_request))
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

L4_Word_t qemu_t::send_mmio_req(uint8_t type, L4_Word_t gpa,
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

    if(raise_event(IQemu_event_io_request))
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

L4_Word_t qemu_t::send_mmio_req(uint8_t type, L4_Word_t gpa,
		       L4_Word_t count, L4_Word_t size, L4_Word_t value,
		       uint8_t dir, uint8_t df, uint8_t value_is_ptr)
{ return 0; }
#endif

#ifdef CONFIG_L4KA_DRIVER_REUSE_QEMU_WITH_PIC

void qemu_t::raise_irq(void)
{
    irq_is_pending = true;
}

void qemu_t::reraise_irq(L4_Word_t vector)
{
    irq_is_pending = true;
}

bool qemu_t::pending_irq(L4_Word_t &vector, L4_Word_t &irq )
{
    CORBA_Environment ipc_env;
    ipc_env = idl4_default_environment;
    
    irq = -1UL;
    vector = -1UL;

    struct irqreq *irqr = &s_pages.sio_page.vcpu_iodata[0].vp_irqreq;
    if(irq_is_pending)
    {
	vector = irqr->pending.vector;
	irq = irqr->pending.irq;
	last_pending_irq = irq;
	last_pending_vector = vector;
	irq_is_pending = false;
	return 1;
    }
    return 0;
}

void qemu_t::init_cmos(void)
{
    u8_t *cmos = s_pages.sio_page.vcpu_iodata[0].cmos_data;

	    cmos[0x15] = 640 & 0xff; //   Base Memory Low Order Byte - Least significant byte
	    cmos[0x16] = (640 >> 8) & 0xff;	//	Base Memory High Order Byte - Most significant byte
	    cmos[0x17] = min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) & 0xff;	//	Extended Memory Low Order Byte - Least significant byte
	    cmos[0x18] = (min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) >> 8) & 0xff;	//	Extended Memory Low Order Byte - Most significant byte
	    cmos[0x30] = min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) & 0xff;
	    cmos[0x31] = (min(((resourcemon_shared.phys_size >> 10) - 0x400), 0xffffUL) >> 8) & 0xff;	//	Extended Memory Low Order Byte - Most significant byte
	    cmos[0x34] = min(((resourcemon_shared.phys_size > 0x01000000) ? 
			   (resourcemon_shared.phys_size >> 16) - 0x100 : 0), 0xffffUL) & 0xff;	//	Actual Extended Memory (in 64KByte) - Least significant byte
	    cmos[0x35] = (min(((resourcemon_shared.phys_size > 0x01000000) ? 
			    (resourcemon_shared.phys_size >> 16) - 0x0100 : 0), 0xffffUL) >> 8) & 0xff;	//	Actual Extended Memory (in 64KByte) - Most significant byte

/* Afterburner CMOS extensions */
	    u8_t val;
	    vcpu_t &vcpu = get_vcpu();
	    word_t paddr = vcpu.get_wedge_paddr();
	    val = (paddr >> 20); 
	    cmos[0x80] = val;

	    paddr = ROUND_UP((vcpu.get_wedge_end_paddr() - vcpu.get_wedge_paddr()), MB(4));
	    val = (paddr >> 20); 
	    cmos[0x81] = val;
}

bool qemu_t::lock_aquire(void)
{
    L4_Time_t timeout = L4_TimePeriod(10);
    struct irqreq *irqreq = &s_pages.sio_page.vcpu_iodata[0].vp_irqreq;
    irqreq->lock.flag_vmm = 1;
    irqreq->lock.turn = 1;
    while(irqreq->lock.flag_qemu && irqreq->lock.turn == 1)
	L4_Sleep(timeout);
    return true;
}

bool qemu_t::lock_release(void)
{
    struct irqreq * irqreq = &s_pages.sio_page.vcpu_iodata[0].vp_irqreq;
    irqreq->lock.flag_vmm = 0;
    return true;
}
#endif

/*********************************************************************
 *
 * Copyright (C) 2005,2007-2008  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/vm.cc
 * Description:   The L4 state machine for implementing the idle loop,
 *                and the concept of "returning to user".  When returning
 *                to user, enters a dispatch loop, for handling L4 messages.
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

#include <debug.h>
#include <console.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kip.h>
#include <l4/arch.h>
#include <device/portio.h>
#include <l4/ia32/tracebuffer.h>

#include INC_ARCH(ia32.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(hvm/message.h)

extern void handle_cpuid( frame_t *frame );

bool vcpu_t::startup_vcpu(word_t startup_ip, word_t startup_sp, word_t boot_id, bool bsp)
{
    monitor_gtid = L4_Myself();
    monitor_ltid = L4_MyLocalId();

    // init vcpu
    main_gtid = get_hthread_manager()->thread_id_allocate();
    main_info.set_tid(main_gtid);

    ASSERT( main_gtid != L4_nilthread );
    
    if( !get_vm()->init_guest() ) {
	printf( "Unable to load guest module.\n");
	return false;
    }

    L4_ThreadId_t scheduler, pager;
    L4_Error_t last_error;
    L4_Word_t irq_prio;
    L4_MsgTag_t tag;
    
    scheduler = monitor_gtid;
    pager = monitor_gtid;
    // create thread
    last_error = ThreadControl( main_gtid, main_gtid, L4_Myself(), L4_nilthread, 0xeffff000 );
    if( last_error != L4_ErrOk )
    {
	printf( "Error: failure creating first thread, tid %t scheduler tid %d L4 error: %d\n",
		main_gtid, scheduler, last_error);
	return false;
    }
    
    // create address space
    last_error = SpaceControl( main_gtid, 1 << 30, L4_Fpage( 0xefffe000, 0x1000 ), 
			       L4_Fpage( 0xeffff000, 0x1000 ), L4_nilthread );
    if( last_error != L4_ErrOk )
    {
	printf( "Error: failure creating space, tid %t, L4 error code %d\n", main_gtid, last_error);
	goto err_space_control;
    }

    // set the thread's priority
    if( !L4_Set_Priority( main_gtid, get_vm()->get_prio() ) )
    {
	printf( "Error: failure setting guest's priority, tid %t, L4 error code %d\n", main_gtid, last_error);
	goto err_priority;
    }

    // make the thread valid
    last_error = ThreadControl( main_gtid, main_gtid, scheduler, pager, -1UL);
    if( last_error != L4_ErrOk ) {
	printf( "Error: failure starting thread guest's priority, tid %t, L4 error code %d\n", main_gtid, last_error);
	goto err_valid;
    }

    if( !backend_preboot( NULL ) ) {
	printf( "Error: backend_preboot failed\n");
	goto err_activate;
    }

    // start the thread
    L4_Set_Priority( main_gtid, 50 );
    printf( "Startup IP %x\n", get_vm()->entry_ip);
    if( get_vm()->guest_kernel_module == NULL ) printf( "Starting in real mode\n");
    
    // cached cr0, see load_startup_reply
    if(get_vm()->guest_kernel_module == NULL)
	get_cpu().cr0.x.raw = 0x00000000;
    else
	get_cpu().cr0.x.raw = 0x00000031;
    

    main_info.mr_save.load_startup_reply( get_vm()->entry_ip, get_vm()->entry_sp, 
					  get_vm()->entry_cs, get_vm()->entry_ss,
					  (get_vm()->guest_kernel_module == NULL));
    main_info.mr_save.load();
    
    DEBUGGER_ENTER("STARTUP");
    
    tag = L4_Send( main_gtid );
    if (L4_IpcFailed( tag ))
    {
	printf( "Error: failure sending startup IPC to %t\n", main_gtid);
	L4_KDB_Enter("failed");
	goto err_activate;
    }
        
    dprintf(debug_startup, "Main thread initialized TID %t VCPU %d\n", main_gtid, cpu_id);

    // Configure ctrlxfer items for all exit reasons
    L4_CtrlXferItem_t item; 
    L4_Msg_t ctrlxfer_msg;
    L4_Word_t dummy, old_control;
    L4_ThreadId_t old_tid;
    // by default, always send GPREGS
    for (word_t fault=0;fault < L4_NUM_BASIC_EXIT_REASONS; fault++) 
    {
	L4_FaultConfCtrlXferItemInit(&item, fault+L4_EXIT_REASON_OFFSET, L4_CTRLXFER_GPREGS_MASK);    
	L4_Clear (&ctrlxfer_msg);
	L4_Append(&ctrlxfer_msg, item.raw[0]);
	L4_Load (&ctrlxfer_msg);
	L4_ExchangeRegisters (main_gtid, L4_EXREGS_CTRLXFER_CONF_FLAG, 0, 0 , 0, 0, L4_nilthread,
			      &old_control, &dummy, &dummy, &dummy, &dummy, &old_tid);
    }

    irq_prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER;
    irq_ltid = irq_init( irq_prio, L4_Pager(), this);
    if( L4_IsNilThread(irq_ltid) )
	return false;
    irq_gtid = L4_GlobalId( irq_ltid );
    irq_info.set_tid(irq_ltid);
    dprintf(debug_startup, "IRQ thread initialized TID %t VCPU %d\n", irq_gtid, cpu_id);
    
    return true;

 err_space_control:
 err_priority:
 err_valid:
 err_activate:
    
    // delete thread and space
    ThreadControl(main_gtid, L4_nilthread, L4_nilthread, L4_nilthread, -1UL);
    return false;

	
}   


void mr_save_t::load_startup_reply(word_t start_ip, word_t start_sp, word_t start_cs, word_t start_ss, bool rm)
{
    if( rm ) 
    {
	gpregs_item.gprs.eip = start_ip;
	gpregs_item.gprs.esp = start_sp;
	// if the disk is larger than 3 MB, assume it is a hard disk
	if( get_vm()->ramdisk_size >= MB(3) ) 
	{
	    gpregs_item.gprs.edx = 0x80;
	} else 
	{
	    gpregs_item.gprs.edx = 0;
	}
	
	/* CS */
	L4_SegmentCtrlXferItemSet(&seg_item[0], 0, start_cs);
	L4_SegmentCtrlXferItemSet(&seg_item[0], 1, start_cs << 4);
	
	L4_SegmentCtrlXferItemSet(&seg_item[1], 0, start_ss);
	L4_SegmentCtrlXferItemSet(&seg_item[1], 1, start_ss << 4);

    } 
    else 
    { 
	// protected mode
	gpregs_item.gprs.eip = start_ip;
	gpregs_item.gprs.esi = 0x9022;

	
	L4_SegmentCtrlXferItemSet(&seg_item[0], 0, 1 << 3);
	L4_SegmentCtrlXferItemSet(&seg_item[0], 1, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[0], 2, ~0UL);
	L4_SegmentCtrlXferItemSet(&seg_item[0], 3, 0x0c099);

	L4_SegmentCtrlXferItemSet(&seg_item[1], 0, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[1], 3, 0x10000);

	L4_SegmentCtrlXferItemSet(&seg_item[2], 0, 2 << 3);
	L4_SegmentCtrlXferItemSet(&seg_item[2], 1, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[2], 2, ~0UL);
	L4_SegmentCtrlXferItemSet(&seg_item[2], 3, 0x0c093);
	
	L4_SegmentCtrlXferItemSet(&seg_item[3], 0, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[3], 3, 0x10000);
	
	L4_SegmentCtrlXferItemSet(&seg_item[4], 0, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[4], 3, 0x10000);
	
	L4_SegmentCtrlXferItemSet(&seg_item[5], 0, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[5], 3, 0x10000);
	
	L4_SegmentCtrlXferItemSet(&seg_item[6], 0, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[6], 3, 0x0808b);
	
	L4_SegmentCtrlXferItemSet(&seg_item[7], 0, 0);
	L4_SegmentCtrlXferItemSet(&seg_item[7], 3, 0x18003);

	L4_RegCtrlXferItemSet(&cr_item[0], L4_CTRLXFER_CREGS_CR0, 0x00000031);
    }
    append_gpregs_item();
}

bool handle_register_write(L4_Word_t reg, L4_VirtFaultOperand_t operand, L4_Word_t value)
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if( operand.X.type == L4_OperandMemory ) {
	printf("unhandled write to register %x with memory operand %x val %x ip %x\n", 
	       reg, operand, value, vcpu_mrs->gpregs_item.gprs.eip); 
	DEBUGGER_ENTER("UNIMPLEMENTED");
	return false;
    }

    dprintf(debug_hvm_fault,  "write to register %x val %x\n", vcpu_mrs->gpregs_item.gprs.eip, reg, value); 
    
    switch (reg)
    {
    case L4_CTRLXFER_REG_ID(L4_CTRLXFER_CREGS_ID, L4_CTRLXFER_CREGS_CR0):
	get_cpu().cr0.x.raw = value;
	break;
    case L4_CTRLXFER_REG_ID(L4_CTRLXFER_CREGS_ID, L4_CTRLXFER_CREGS_CR3):
	get_cpu().cr3.x.raw = value;
	break;
    default:
	printf("unhandled write to register %x operand %x val %x ip %x\n", 
	       reg, operand, value, vcpu_mrs->gpregs_item.gprs.eip); 
	DEBUGGER_ENTER("UNIMPLEMENTED");
	break;
    }
    
    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();
    
    return true;
}

bool handle_register_read(L4_Word_t reg, L4_VirtFaultOperand_t operand)
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if( operand.X.type == L4_OperandMemory ) {
	printf("unhandled read from register %x with memory operand %x ip %x\n", 
	       reg, operand, vcpu_mrs->gpregs_item.gprs.eip); 
	DEBUGGER_ENTER("UNIMPLEMENTED");
	return false;
    }
    L4_Word_t value;
    L4_StoreMR( 4, &value );

    dprintf(debug_hvm_fault,  "read from register %x operand %x to reg idx %x val %x ip %x", 
	    reg, operand, operand.reg.index, value, vcpu_mrs->gpregs_item.gprs.eip); 

    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->gpregs_item.gprs.reg[operand.reg.index] = value;
    vcpu_mrs->append_gpregs_item();

    return true;
}


bool handle_instruction(L4_Word_t instruction)
{
    u64_t value64;
    L4_Msg_t ctrlxfer_msg;
    frame_t frame;

    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    dprintf(debug_hvm_fault, "instruction fault %x ip %x\n", instruction, vcpu_mrs->gpregs_item.gprs.eip);


    L4_Clear(&ctrlxfer_msg);

    switch( instruction ) {
	case L4_VcpuIns_cpuid:
	    frame.x.fields.eax = vcpu_mrs->gpregs_item.gprs.eax;

	    handle_cpuid( &frame );
	    
	    vcpu_mrs->gpregs_item.gprs.eax = frame.x.fields.eax;
	    vcpu_mrs->gpregs_item.gprs.ecx = frame.x.fields.ecx;
	    vcpu_mrs->gpregs_item.gprs.edx = frame.x.fields.edx;
	    vcpu_mrs->gpregs_item.gprs.ebx = frame.x.fields.ebx;
	    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
	    vcpu_mrs->append_gpregs_item();
	    
	    return true;

	case L4_VcpuIns_hlt:
	    // wait until next interrupt arrives
	    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
	    return false;

	case L4_VcpuIns_invlpg:
	    DEBUGGER_ENTER("UNTESTED INVLPG");
	    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
	    vcpu_mrs->append_gpregs_item();

	    return true;

	case L4_VcpuIns_rdtsc:
	    value64 = ia32_rdtsc();
	    vcpu_mrs->gpregs_item.gprs.eax = value64;
	    vcpu_mrs->gpregs_item.gprs.edx = value64 >> 32;
	    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
	    vcpu_mrs->append_gpregs_item();
	    return true;

	case L4_VcpuIns_monitor:
	case L4_VcpuIns_mwait:
	case L4_VcpuIns_pause:
	    DEBUGGER_ENTER("UNTESTED MONITOR/MWAIT/PAUSE");
	    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
	    vcpu_mrs->append_gpregs_item();

	    return true;

	default:
	    printf("%x: unhandled instruction %x\n", vcpu_mrs->gpregs_item.gprs.eip, instruction);
	    DEBUGGER_ENTER("monitor: unhandled instruction");
	    return false;
    }
}


// handle string io for various types
template <typename T>
bool string_io( L4_Word_t port, L4_Word_t count, L4_Word_t mem_addr, bool write )
{
    T *buf = (T*)mem_addr;
    u32_t tmp;
    for(u32_t i=0;i<count;i++) {
	if(write) {
	    tmp = (u32_t) *(buf++);
	    if(!portio_write( port, tmp, sizeof(T)*8) )
	       return false;
	}
	else {
	    if(!portio_read( port, tmp, sizeof(T)*8) )
		return false;
	    *(buf++) = (T)tmp;
	}
    }
    return true;
}



bool handle_io_write(L4_VirtFaultIO_t io, L4_VirtFaultOperand_t operand, L4_Word_t value)
{
    L4_Word_t mem_addr;
    L4_Word_t paddr;
    L4_Word_t ecx = 1;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if( io.X.port != 0xcf8 && io.X.port != 0x3f8 )
	dprintf(debug_hvm_io, "%x: write to io port %x value %x\n", vcpu_mrs->gpregs_item.gprs.eip, io.X.port, value);

#if 1
    if( io.X.port >= 0x400 && io.X.port <= 0x403 ) { // ROMBIOS debug ports
	printf("%c", (char)value);
    }
    else if( io.X.port == 0xe9 )  // VGABIOS debug port
	printf("%c", (char)value);
    else 
#endif
	{
	switch( operand.X.type ) {

	case L4_OperandRegister:
	    if( !portio_write( io.X.port, value & ((2 << io.X.access_size-1) - 1),
			       io.X.access_size ) ) {
		// TODO inject exception?
		printf("%x: write to io port %x value %x failed\n", vcpu_mrs->gpregs_item.gprs.eip, io.X.port, value);
		//DEBUGGER_ENTER("monitor: io write failed");
		//return false;
	    }
	    break;

	case L4_OperandMemory:
	    DEBUGGER_ENTER("io write with memory operand!");
	    if(get_cpu().cr0.protected_mode_enabled())
		DEBUGGER_ENTER("String IO write with pe mode");

	    mem_addr=value;
	    if( io.X.rep )
		ecx = vcpu_mrs->gpregs_item.gprs.ecx;

	    paddr = get_vcpu().get_map_addr( mem_addr );

	    switch( io.X.access_size ) {
	    case 8:
		if(!string_io<u8_t>(io.X.port, ecx & 0xffff, paddr, true))
		    goto err_io;
		break;

	    case 16:
		if(!string_io<u16_t>(io.X.port, ecx & 0xffff, paddr, true))
		    goto err_io;
		break;

	    case 32:
		if(!string_io<u32_t>(io.X.port, ecx & 0xffff, paddr, true))
		    goto err_io;
		break;

	    default:
		printf( "Invalid I/O port size %d\n", io.X.access_size);
		DEBUGGER_ENTER("monitor: unhandled string io write");
	    }

	    vcpu_mrs->gpregs_item.gprs.esi +=  + 2*(ecx & 0xffff);
	    break;

	err_io:
	    printf("%x: string write to io port %x value %x failed\n", vcpu_mrs->gpregs_item.gprs.eip, io.X.port, value);
	    break;

	default:
	    DEBUGGER_ENTER("monitor: unhandled io write");
	}
    }

    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();

    return true;
}

bool handle_io_read(L4_VirtFaultIO_t io, L4_VirtFaultOperand_t operand, L4_Word_t mem_addr)
{
    L4_Word_t paddr = 0;
    L4_Word_t count, size = 0;
    word_t value = 0;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if(io.X.port != 0xcfc && io.X.port != 0x3fd && io.X.port != 0x64 )
	dprintf(debug_hvm_io, "%x: read from io port %x\n", vcpu_mrs->gpregs_item.gprs.eip, io.X.port);

    switch( operand.X.type ) 
    {

    case L4_OperandRegister:
	if( !portio_read( io.X.port, value, io.X.access_size ) ) {
	    // TODO inject exception?
	    printf("%x: read from io port %x failed \n", vcpu_mrs->gpregs_item.gprs.eip, io.X.port);
	}
	ASSERT(operand.reg.index == L4_CTRLXFER_REG_ID(L4_CTRLXFER_GPREGS_ID, L4_CTRLXFER_GPREGS_EAX));
	vcpu_mrs->gpregs_item.gprs.eax &= ~((2 << io.X.access_size-1) - 1);
	vcpu_mrs->gpregs_item.gprs.eax |= value;

	break;

    case L4_OperandMemory:

	if( io.X.rep ) {
	    count = vcpu_mrs->gpregs_item.gprs.ecx;
	    if( get_cpu().cr0.real_mode())
		count &= 0xFFFF;
	}
	else
	    count = 1;

	if( get_cpu().cr0.protected_mode_enabled() ) 
	{
	    DEBUGGER_ENTER("io read with memory operand in proctected mode");
#if 0
	    if(cr0.paging_enabled() ) {
		/* paging enabled, lookup paddr in pagetable */
		L4_Word_t attr;
		paddr = gva_to_gpa(mem_addr, attr);

		if(!paddr) {
		    L4_VirtFaultException_t pf_except;
		    //L4_Word_t instr_len = this->get_instr_len();
		    pf_except.raw = 0;
		    pf_except.X.vector = 14; // PF
		    pf_except.X.has_err_code = 1;
		    pf_except.X.type = L4_ExceptionHW;
		    pf_except.X.valid = 1;
		    /* inject PF */
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplyInject;
		    L4_LoadMR( 1, item.raw );
		    L4_LoadMR( 2, pf_except.raw );
		    L4_LoadMR( 3, instr_len );
		    L4_LoadMR( 4, 2 ); // error code

		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetister;
		    item.reg.index = L4_VcpuReg_cr2;
		    L4_LoadMR( 5, item.raw );
		    L4_LoadMR( 6, mem_addr );

		    tag.raw = 0;
		    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
		    tag.X.u = 6;
		    L4_Set_MsgTag( tag );

		    return true;
		}
		/* test if page is writable */
		else {
		    if( !(attr & 0x2) ) {
			printf( "Page is read only\n");
			// Inject GP
			DEBUGGER_ENTER("TODO");
		    }
		}

	    }

	    /* paging disabled, segmentation used */
	    else 
	    {
		// request es_base, es_limit and es_attr
		L4_Word_t es_base, es_limit, es_attr;
		item.raw = 0;
		item.X.type = L4_VirtFaultReplyGetRegister;
		item.reg.index = L4_VcpuReg_es_base;
		L4_LoadMR(1, item.raw);
		item.raw = 0;
		item.X.type = L4_VirtFaultReplyGetRegister;
		item.reg.index = L4_VcpuReg_es_limit;
		L4_LoadMR(2, item.raw);
		item.raw = 0;
		item.X.type = L4_VirtFaultReplyGetRegister;
		item.reg.index = L4_VcpuReg_es_attr;
		L4_LoadMR(3, item.raw);
		tag.raw = 0;
		tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
		tag.X.u = 3;
		L4_Set_MsgTag(tag);

		tag = L4_ReplyWait(this->tid, &vtid);
		
		ASSERT( L4_Label(tag) == (L4_LABEL_VIRT_NORESUME << 4));
		ASSERT(vtid == this->tid);
		L4_StoreMR( 1, &es_base );
		L4_StoreMR( 2, &es_limit );
		L4_StoreMR( 3, &es_attr );
		// TODO: check limit and attributes
		paddr = es_base + edi;
	    }
#endif
	}
	/* real mode, physical address is linear address */
	else {
	    paddr = get_vcpu().get_map_addr( mem_addr );
	}


	switch(io.X.access_size) {
	case 8:
	    if(!string_io<u8_t>(io.X.port, count, paddr, false))
		goto err_io;
	    size = count;
	    break;

	case 16:
	    if(!string_io<u16_t>(io.X.port, count, paddr, false))
		goto err_io;
	    size = count * 2;
	    break;

	case 32:
	    if(!string_io<u32_t>(io.X.port, count, paddr, false))
		goto err_io;
	    size = count * 4;
	    break;

	default:
	    printf( "Invalid I/O port size %d\n",io.X.access_size);
	    DEBUGGER_ENTER("monitor: unhandled string io read");
	}

	if(size > PAGE_SIZE)
	    printf( "WARNING: String IO larger than page size !\n");

	vcpu_mrs->gpregs_item.gprs.edi += size;

	break;

    err_io:
	// TODO: inject GP(0)?
	printf("%x: string read from io port %x failed \n", vcpu_mrs->gpregs_item.gprs.eip, io.X.port);
	break;

    default:
	DEBUGGER_ENTER("monitor: unhandled io read");
    }

    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();
    
    return true;
}

bool handle_msr_write()
{
    L4_Word_t msr;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );

    dprintf(debug_hvm_fault, "write to MSR %x value %x:%x ip %x", 
	    msr, vcpu_mrs->gpregs_item.gprs.edx, vcpu_mrs->gpregs_item.gprs.eax, vcpu_mrs->gpregs_item.gprs.eip);

    switch(msr)
    {
    case 0x174: // sysenter_cs
	L4_MSRCtrlXferItemSet(&vcpu_mrs->msr_item, L4_CTRLXFER_MSR_SYSENTER_CS, vcpu_mrs->gpregs_item.gprs.eax);
	break;
    case 0x175: // sysenter_esp
	L4_MSRCtrlXferItemSet(&vcpu_mrs->msr_item, L4_CTRLXFER_MSR_SYSENTER_ESP, vcpu_mrs->gpregs_item.gprs.eax);
	break;
    case 0x176: // sysenter_eip
	L4_MSRCtrlXferItemSet(&vcpu_mrs->msr_item, L4_CTRLXFER_MSR_SYSENTER_EIP, vcpu_mrs->gpregs_item.gprs.eax);
	break;
    default:
	printf("unhandled write to MSR %x value %x:%x ip %x", 
	       msr, vcpu_mrs->gpregs_item.gprs.edx, vcpu_mrs->gpregs_item.gprs.eax, vcpu_mrs->gpregs_item.gprs.eip );
	DEBUGGER_ENTER("UNIMPLEMENTED");
	break;
    }
    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();
    
    return true;
}

bool handle_msr_read()
{
    L4_Word_t msr;
    L4_Word_t value1, value2;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );
    L4_StoreMR( 3, &value1 );
    L4_StoreMR( 4, &value2 );

    dprintf(debug_hvm_fault, "read from MSR %x value %x:%x ip %x", 
	    msr, value2, value1, vcpu_mrs->gpregs_item.gprs.eip);

    vcpu_mrs->gpregs_item.gprs.eax = value1;
    vcpu_mrs->gpregs_item.gprs.eax = value2;
    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();

    return true;
}

bool handle_unknown_msr_write()
{
    L4_Word_t msr;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );
    
    printf("unhandled write to MSR %x value %x:%x ip %x", 
	   msr, vcpu_mrs->gpregs_item.gprs.edx, vcpu_mrs->gpregs_item.gprs.eax, vcpu_mrs->gpregs_item.gprs.eip );

    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();
    return true;
}

bool handle_unknown_msr_read()
{
    L4_Word_t msr;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );
    
    printf("read from unknown MSR %x ip %x", msr, vcpu_mrs->gpregs_item.gprs.eip );
    DEBUGGER_ENTER("UNTESTED UNKNOWN MSR READ");


    switch (msr)
    {
    case 0x1a0:			// IA32_MISC_ENABLE
	vcpu_mrs->gpregs_item.gprs.eax = 0xc00;
	vcpu_mrs->gpregs_item.gprs.eax = 0;
    break;
    default:
	printf("unhandled read from MSR %x ip %x", 
	       msr, vcpu_mrs->gpregs_item.gprs.eip );
	vcpu_mrs->gpregs_item.gprs.eax = 0;
	vcpu_mrs->gpregs_item.gprs.eax = 0;
    }

    vcpu_mrs->gpregs_item.gprs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();

    return true;
}


typedef struct
{
    u16_t ip;
    u16_t cs;
} ia32_ive_t;

bool vm8086_interrupt_emulation(word_t vector, bool hw)
{
    L4_MsgTag_t tag;
    L4_Word_t stack_addr;
    u16_t *stack;
    ia32_ive_t *int_vector;
    
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    // Request additional VCPU state (cs,ss)
    vcpu_mrs->seg_item[0].item.num_regs = 4;
    vcpu_mrs->seg_item[0].item.mask = 0xf;
    vcpu_mrs->seg_item[0].item.C=1;

    vcpu_mrs->seg_item[1].item.num_regs = 4;
    vcpu_mrs->seg_item[1].item.mask = 0xf;
    vcpu_mrs->seg_item[1].item.C=0;

    
    vcpu_mrs->load();
    DEBUGGER_ENTER("UNTESTED REQUEST STATE");
    // Read new items via exregs
    L4_ReadCtrlXferItems(get_vcpu().main_gtid);
    
    tag = L4_MsgTag();
    L4_StoreMRs(L4_UntypedWords(tag)+1, 5, (L4_Word_t*)&vcpu_mrs->seg_item[0]);
    L4_StoreMRs(L4_UntypedWords(tag)+6, 5, (L4_Word_t*)&vcpu_mrs->seg_item[1]);
    
    ASSERT(vcpu_mrs->seg_item[0].item.id == 4);
    ASSERT(vcpu_mrs->seg_item[1].item.id == 5);

    if(!(vcpu_mrs->gpregs_item.gprs.eflags & 0x200) && hw) {
	printf( "WARNING: hw interrupt injection with if flag disabled !!!\n");
	printf( "eflags is: %x\n", vcpu_mrs->gpregs_item.gprs.eflags);
	goto erply;
    }

    //    printf("\nReceived int vector %x\n", vector);
    //    printf("Got: sp:%x, efl:%x, ip:%x, cs:%x, ss:%x\n", sp, eflags, ip, cs, ss);
    stack_addr = (vcpu_mrs->seg_item[1].regs.segreg << 4) + (vcpu_mrs->gpregs_item.gprs.esp & 0xffff);
    stack = (u16_t*) get_vcpu().get_map_addr( stack_addr );
    // push eflags, cs and ip onto the stack
    *(--stack) = vcpu_mrs->gpregs_item.gprs.eflags & 0xffff;
    *(--stack) = vcpu_mrs->seg_item[0].regs.segreg & 0xffff;
    if (hw)
	*(--stack) = vcpu_mrs->gpregs_item.gprs.eip & 0xffff;
    else
	*(--stack) = (vcpu_mrs->gpregs_item.gprs.eip + vcpu_mrs->instr_len) & 0xffff;

    if( vcpu_mrs->gpregs_item.gprs.esp-6 < 0 )
	DEBUGGER_ENTER("stackpointer below segment");
    
    // get entry in interrupt vector table from guest
    int_vector = (ia32_ive_t*) get_vcpu().get_map_addr( vector*4 );
    dprintf(debug_hvm_irq, "Ii: %x (%c), entry: %x, %x at: %x\n", 
	    vector, hw ? 'h' : 's',  int_vector->ip, int_vector->cs, 
	    (vcpu_mrs->seg_item[0].regs.base + vcpu_mrs->gpregs_item.gprs.eip));
    

    vcpu_mrs->gpregs_item.gprs.eip = int_vector->ip;
    vcpu_mrs->gpregs_item.gprs.esp -= 6;
    vcpu_mrs->gpregs_item.gprs.eflags &= 0x200;

    vcpu_mrs->append_gpregs_item();

    L4_SegmentCtrlXferItemSet(&vcpu_mrs->seg_item[0], L4_CTRLXFER_CSREGS_CS, int_vector->cs);
    L4_SegmentCtrlXferItemSet(&vcpu_mrs->seg_item[0], L4_CTRLXFER_CSREGS_CS_BASE, (int_vector->cs << 4 ));
    
 erply:

    return true;
}


bool backend_sync_deliver_vector( L4_Word_t vector, bool old_int_state, bool use_error_code, L4_Word_t error_code )
{
    L4_VirtFaultException_t except;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if( get_cpu().cr0.real_mode() ) 
    {
	// Real mode, emulate interrupt injection
	return vm8086_interrupt_emulation(vector, true);
    }

    except.raw = 0;
    except.X.vector = vector;
    except.X.type = L4_ExceptionExtInt;
    except.X.valid = 1;

    printf("backend_sync_deliver_vector %d err %d\n", vector, error_code); 
    DEBUGGER_ENTER("UNTESTED INJECT IRQ");

    L4_ExcInjectCtrlXferItemSet(&vcpu_mrs->exc_item, except.raw, error_code, 0);

    return true;
}

bool handle_real_mode_fault()
{
    u8_t *linear_ip;
    word_t data_size		= 16;
    word_t addr_size		= 16;
    u8_t modrm;
    word_t operand_addr		= -1UL;
    word_t operand_data		= 0UL;
    bool rep			= false;
    bool seg_ovr		= false;
    word_t seg_ovr_base         = 0;
    L4_MsgTag_t	tag = L4_MsgTag();
    L4_VirtFaultOperand_t operand;
    L4_VirtFaultIO_t io;
    L4_Word_t mem_addr;

    printf("Real mode fault\n");

    vcpu_t &vcpu = get_vcpu();
    mr_save_t *vcpu_mrs = &vcpu.main_info.mr_save;

    // Request additional VCPU state (cs,ds,es,gs)
    vcpu_mrs->seg_item[0].item.num_regs = 4;
    vcpu_mrs->seg_item[0].item.mask = 0xf;
    vcpu_mrs->seg_item[0].item.C=1;
    
    vcpu_mrs->seg_item[2].item.num_regs = 4;
    vcpu_mrs->seg_item[2].item.mask = 0xf;
    vcpu_mrs->seg_item[2].item.C=1;
    
    vcpu_mrs->seg_item[3].item.num_regs = 4;
    vcpu_mrs->seg_item[3].item.mask = 0xf;
    vcpu_mrs->seg_item[3].item.C=1;
    
    vcpu_mrs->seg_item[5].item.num_regs = 4;
    vcpu_mrs->seg_item[5].item.mask = 0xf;
    
    vcpu_mrs->load();
    DEBUGGER_ENTER("UNTESTED REQUEST STATE REAL MODE");
    // Read new items via exregs
    L4_ReadCtrlXferItems(vcpu.main_gtid);

    tag = L4_MsgTag();
    L4_StoreMRs(L4_UntypedWords(tag)+1, 5, (L4_Word_t*)&vcpu_mrs->seg_item[0]);
    L4_StoreMRs(L4_UntypedWords(tag)+6, 5, (L4_Word_t*)&vcpu_mrs->seg_item[2]);
    L4_StoreMRs(L4_UntypedWords(tag)+11, 5, (L4_Word_t*)&vcpu_mrs->seg_item[3]);
    L4_StoreMRs(L4_UntypedWords(tag)+16, 5, (L4_Word_t*)&vcpu_mrs->seg_item[5]);
    ASSERT(vcpu_mrs->seg_item[0].item.id == 4);
    ASSERT(vcpu_mrs->seg_item[2].item.id == 6);
    ASSERT(vcpu_mrs->seg_item[3].item.id == 7);
    ASSERT(vcpu_mrs->seg_item[5].item.id == 9);

    linear_ip = (u8_t*) (vcpu_mrs->gpregs_item.gprs.eip + vcpu_mrs->seg_item[0].regs.base);

    // Read the faulting instruction.

    // Strip prefixes.
    while (*linear_ip == 0x26 
	   || *linear_ip == 0x66 
	   || *linear_ip == 0x67 
	   || *linear_ip == 0xf3
	   || *linear_ip == 0xf2)
	{
	    switch (*(linear_ip++))
		{
		case 0x26:
		    seg_ovr = true;
		    seg_ovr_base = vcpu_mrs->seg_item[3].regs.base;//vmcs->gs.es_base;
		    break;
		case 0x66:
		    data_size = 32;
		    break;
		case 0x67:
		    addr_size = 32;
		    break;
		case 0xf3:
		    rep = true;
		    break;
		case 0xf2:
		    rep = true;
		    break;
		}
	}
    
    // Decode instruction.
    switch (*linear_ip)
	{
	case 0x0f:				// mov, etc.
	    switch (*(linear_ip + 1))
		{
		case 0x00:
		    printf("lldt?\n");

		    return false;
		    break;

		case 0x01:			// lgdt/lidt/lmsw.
		    modrm = *(linear_ip + 2);

		    switch (modrm & 0xc0)
			{
			case 0x00:
			    if (addr_size == 32)
				{
				    switch (modrm & 0x7)
					{
					case 0x0:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.eax;
					    break;
					case 0x1:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.ecx;
					    break;
					case 0x2:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.edx;
					    break;
					case 0x3:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.ebx;
					    break;
					case 0x6:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.esi;
					    break;
					case 0x7:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.edi;
					    break;
					case 0x5:
					    operand_addr = *((u32_t *) (linear_ip + 3));
					    break;
					default:
					    // Other operands not supported yet.
					    return false;
					}
				}
			    else
				{
				    switch (modrm & 0x7)
					{
					case 0x4:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.esi;
					    break;
					case 0x5:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.edi;
					    break;
					case 0x7:
					    operand_addr = vcpu_mrs->gpregs_item.gprs.ebx;
					    break;
					case 0x6:
					    operand_addr = *((u16_t *) (linear_ip + 3));
					    break;
					default:
					    // Other operands not supported yet.
					    return false;
					}

				    operand_addr &= 0xffff;
				    //operand_addr += vmcs->gs.ds_sel << 4;
				    L4_KDB_Enter("rewrite");
				}
			    break;

			case 0xc0:
			    {
				switch (modrm & 0x7)
				    {
				    case 0x0:
					operand_data = vcpu_mrs->gpregs_item.gprs.eax;
					break;
				    case 0x1:
					operand_data = vcpu_mrs->gpregs_item.gprs.ecx;
					break;
				    case 0x2:
					operand_data = vcpu_mrs->gpregs_item.gprs.edx;
					break;
				    case 0x3:
					operand_data = vcpu_mrs->gpregs_item.gprs.ebx;
					break;
				    case 0x4:
					operand_data = vcpu_mrs->gpregs_item.gprs.esp;
					break;
				    case 0x5:
					operand_data = vcpu_mrs->gpregs_item.gprs.ebp;
					break;
				    case 0x6:
					operand_data = vcpu_mrs->gpregs_item.gprs.esi;
					break;
				    case 0x7:
					operand_data = vcpu_mrs->gpregs_item.gprs.edi;
					break;
				    }
			    }
			    break;

			default:
			    // Other operands not supported yet.
			    return false;
			}

		    switch (modrm & 0x38)
			{
			    printf("modrm & 0x38 unimplemented\n");
#if 0
			case 0x10:			// lgdt.
			    vmcs->gs.gdtr_lim	= *((u16_t *) operand_addr);
			    operand_data		= *((u32_t *) (operand_addr + 2));
			    if (data_size < 32)
				operand_data &= 0x00ffffff;
			    vmcs->gs.gdtr_base	= operand_data;
			    break;

			case 0x18:			// lidt.
			    vmcs->gs.idtr_lim	= *((u16_t *) operand_addr);
			    operand_data		= *((u32_t *) (operand_addr + 2));
			    if (data_size < 32)
				operand_data &= 0x00ffffff;
			    vmcs->gs.idtr_base	= operand_data;
			    break;

			case 0x30:			// lmsw.
			    if (operand_addr != -1UL)
				operand_data = *((u16_t *) operand_addr);

			    operand.raw		= 0;
			    operand.X.type		= virt_msg_operand_t::o_lmsw;
			    operand.imm_value	= operand_data & 0xffff;

			    msg_handler->send_register_fault
				(virt_vcpu_t::r_cr0, true, &operand);
			    return true;
#endif
			default:
			    return false;
			}

		    printf("TODO");
		    //vmcs->gs.rip = guest_ip + vmcs->exitinfo.instr_len;
		    return true;

		case 0x20:			// mov cr, x.
		case 0x22:			// mov x, cr.
		    modrm = *(linear_ip + 2);

		    if (modrm & 0xc0 != 0xc0)
			return false;

		    operand.raw		= 0;
		    operand.X.type	= L4_OperandRegister;
		    
		    
		    DEBUGGER_ENTER("UNTESTED EAX REF 1");
		    operand.reg.index	= L4_CTRLXFER_GPREGS_EAX + (modrm & 0x7);


		    //		    msg_handler->send_register_fault
		    //	((virt_vcpu_t::vcpu_reg_e)
		    //	 (virt_vcpu_t::r_cr0 + ((modrm >> 3) & 0x7)),
		    //	 *(linear_ip + 1) == 0x22,
		    //	 &operand);
		    if( *(linear_ip+1) == 0x22) // register write
			return handle_register_write( L4_CTRLXFER_REG_ID(L4_CTRLXFER_CREGS_ID, 0) + ((modrm >> 3) & 0x7),
						      operand, vcpu_mrs->gpregs_item.gprs.reg[(modrm & 0x7)]);
		    else
			//return 
			printf("todo handle register read\n");
		}

	    return false;

	case 0x6c:				// insb	 dx, mem	
	case 0x6e:				// outsb  dx,mem      
	    data_size = 8;
	    // fall through
	case 0x6d:				// insd	dx, mem
	case 0x6f:				// outsd dx, mem
	    io.raw		= 0;
	    io.X.rep		= rep;
	    io.X.port		= vcpu_mrs->gpregs_item.gprs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandMemory;


	    if (seg_ovr)
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (seg_ovr_base + (vcpu_mrs->gpregs_item.gprs.esi & 0xffff))
		    : (seg_ovr_base + (vcpu_mrs->gpregs_item.gprs.edi & 0xffff));
	    else
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (vcpu_mrs->seg_item[2].regs.base + (vcpu_mrs->gpregs_item.gprs.esi & 0xffff))
		    : (vcpu_mrs->seg_item[3].regs.base + (vcpu_mrs->gpregs_item.gprs.edi & 0xffff));

	    if(*linear_ip >= 0x6e) // write
		return handle_io_write(io, operand, mem_addr);
	    else
		return handle_io_read(io, operand, mem_addr);

	case 0xcc:				// int 3.
	case 0xcd:				// int n.
	    return vm8086_interrupt_emulation( *linear_ip == 0xcc ? 3 : *(linear_ip + 1), false);

	case 0xe4:				// inb n.
	case 0xe6:				// outb n.
	    data_size = 8;
	    // fall through
	case 0xe5:				// in n.
	case 0xe7:				// out n.
	    io.raw			= 0;
	    io.X.rep		= rep;
	    io.X.port		= *(linear_ip + 1);
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandRegister;
	    DEBUGGER_ENTER("UNTESTED EAX REF 2");
	    operand.reg.index	= L4_CTRLXFER_GPREGS_EAX;
	    
	    if(*linear_ip >= 0xe6) // write
		return handle_io_write(io, operand, vcpu_mrs->gpregs_item.gprs.eax);
	    else		
		return handle_io_read(io, operand, 0);
	case 0xec:				// inb dx.
	case 0xee:				// outb dx.
	    data_size = 8;
	    // fall through
	case 0xed:				// in dx.
	case 0xef:				// out dx.
	    io.raw			= 0;
	    io.X.rep		= rep;
	    io.X.port		= vcpu_mrs->gpregs_item.gprs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandRegister;
	    DEBUGGER_ENTER("UNTESTED EAX REF 3");
	    operand.reg.index	= L4_CTRLXFER_GPREGS_EAX;

	    if(*linear_ip >= 0xee) // write
		return handle_io_write(io, operand, vcpu_mrs->gpregs_item.gprs.eax);
	    else
		return handle_io_read(io, operand, 0);

	case 0xf4:				// hlt
	    return handle_instruction(L4_VcpuIns_hlt);
	}

    return false;
}

bool handle_exception()
{
    L4_VirtFaultException_t except;
    L4_Word_t err_code = 0;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    L4_Word_t addr = 0;
	    
    L4_StoreMR( 2, &except.raw );

    //  If guest is in real mode do special fault handling
    if( !get_cpu().cr0.protected_mode_enabled() && (except.X.vector == L4_ExceptionGP))
    {
	//printf("handling real mode\n");
	if(handle_real_mode_fault())
	    return true;
    }

    if( except.X.has_err_code ) {
	L4_StoreMR( 3, &err_code );
	// page fault: store address
	if( except.X.vector == 14 ) 
	{

	    L4_StoreMR( 4, &addr );
	    L4_RegCtrlXferItemSet(&vcpu_mrs->cr_item[2], L4_CTRLXFER_CREGS_CR2, addr);

	}
    }

    dprintf(debug_hvm_fault,  "exception %x %x %x ip %x\n", except.raw, err_code, addr, vcpu_mrs->gpregs_item.gprs.eip); 
    DEBUGGER_ENTER("UNTESTED INJECT IRQ");

    L4_ExcInjectCtrlXferItemSet(&vcpu_mrs->exc_item, except.raw, 0, 0);

    return true;
}



bool backend_handle_vfault_message()
{
    L4_MsgTag_t	tag = L4_MsgTag();
    L4_Word_t reg;
    L4_VirtFaultOperand_t operand;
    L4_VirtFaultIO_t io;
    L4_Word_t value;
    
    vcpu_t &vcpu = get_vcpu();
    mr_save_t *vcpu_mrs = &vcpu.main_info.mr_save;
    
    // store GPRegs
    L4_StoreMRs(L4_UntypedWords(tag)+1, L4_CTRLXFER_GPREGS_SIZE+1, (L4_Word_t*)&vcpu_mrs->gpregs_item);
    ASSERT((vcpu_mrs->gpregs_item.item.__type) == 0x06 ); // ctrlxfer item
    ASSERT((vcpu_mrs->gpregs_item.item.id) == 0); // gpregs
    
    L4_StoreMR( 1, &(vcpu_mrs->instr_len) );

    switch( L4_Label( tag )) 
    {
	case (L4_LABEL_REGISTER_FAULT << 4) | 0x2 | 0x8:
	// TODO implement debug registers in kernel and remove this
	case (L4_LABEL_REGISTER_FAULT << 4) | 0x2:
	    L4_StoreMR(2, &reg);
	    L4_StoreMR(3, &operand.raw);
	    L4_StoreMR(4, &value);
	    return handle_register_write(reg, operand, value);

	case (L4_LABEL_REGISTER_FAULT << 4) | 0x4 | 0x8:
	    L4_StoreMR(2, &reg);
	    L4_StoreMR(3, &operand.raw);
	    return handle_register_read(reg, operand);

	case L4_LABEL_INSTRUCTION_FAULT << 4:
	    L4_StoreMR(2, &reg);
	    return handle_instruction(reg);

	case (L4_LABEL_EXCEPTION_FAULT << 4):
        case (L4_LABEL_EXCEPTION_FAULT << 4) | 0x8:
	    return handle_exception();

	case (L4_LABEL_IO_FAULT << 4) | 0x2:
	    L4_StoreMR( 2, &io.raw );
	    L4_StoreMR( 3, &operand.raw );
	    L4_StoreMR( 4, &value );
	    return handle_io_write(io, operand, value);

	case (L4_LABEL_IO_FAULT << 4) | 0x4:
	    L4_StoreMR( 2, &io.raw );
	    L4_StoreMR( 3, &operand.raw );
	    L4_StoreMR( 4, &value );
	    return handle_io_read(io, operand, value);

	case (L4_LABEL_MSR_FAULT << 4) | 0x2 | 0x8:
	    return handle_msr_write();

	case (L4_LABEL_MSR_FAULT << 4) | 0x4 | 0x8:
	    return handle_msr_read();

	case (L4_LABEL_MSR_FAULT << 4) | 0x2:
	    return handle_unknown_msr_write();

	case (L4_LABEL_MSR_FAULT << 4) | 0x4:
	    return handle_unknown_msr_read();

	case L4_LABEL_DELAYED_FAULT << 4:	
	    vcpu.interrupt_window_exit = false;
	    //L4_TBUF_RECORD_EVENT(12, "IL delayed fault, deliver irq %d", irq);
	    DEBUGGER_ENTER("UNIMPLEMENTED delayed fault");
	    return true; 
	default:
	    printf( "unhandled message tag %x from %t\n", tag.raw, vcpu.main_gtid);
	    DEBUGGER_ENTER("monitor: unhandled message");
	    return false;
    }
}


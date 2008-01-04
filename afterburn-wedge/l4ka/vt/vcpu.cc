/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
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

#include <l4/schedule.h>
#include <l4/ipc.h>
#include <string.h>
#include <string.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kip.h>
#include <l4/ia32/virt.h>
#include <l4/arch.h>
#include <device/portio.h>
#include <l4/ia32/tracebuffer.h>

#include INC_ARCH(ia32.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(monitor.h)
#include <console.h>
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include <debug.h>
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(vt/message.h)

const bool debug_vfault = 0;
const bool debug_io = 0;
const bool debug_ramdisk = 0;
const bool debug_irq_inject = 0;

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
    last_error = SpaceControl( main_gtid, 1 << 30, L4_Fpage( 0xefffe000, 0x1000 ), L4_Fpage( 0xeffff000, 0x1000 ), L4_nilthread );
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
    main_info.state = thread_state_running;
    main_info.mr_save.load_startup_reply( get_vm()->entry_ip, get_vm()->entry_sp, 
					  get_vm()->entry_cs, get_vm()->entry_ss,
					  (get_vm()->guest_kernel_module == NULL));
  
    // cached cr0, see load_startup_reply
    if(get_vm()->guest_kernel_module == NULL)
	main_info.cr0.x.raw = 0x00000000;
    else
	main_info.cr0.x.raw = 0x00000031;

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
    for(word_t fault=0;fault < L4_NUM_BASIC_EXIT_REASONS; fault++) {
	item = L4_FaultConfCtrlXferItem(fault+L4_EXIT_REASON_OFFSET, L4_CTRLXFER_GPREGS_MASK);    
	L4_Clear (&ctrlxfer_msg);
	L4_Append(&ctrlxfer_msg, item.raw[0]);
	L4_Load (&ctrlxfer_msg);
	L4_ExchangeRegisters (main_gtid, (1<<11), 0, 0 , 0, 0, L4_nilthread,
			      &old_control, &dummy, &dummy, &dummy, &dummy, &old_tid);
    }

    irq_prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ_HANDLER;
    irq_ltid = irq_init( irq_prio, L4_Myself(), this);
    if( L4_IsNilThread(irq_ltid) )
	return false;
    irq_gtid = L4_GlobalId( irq_ltid );
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
    //L4_VirtFaultReplyItem_t item;
    L4_Msg_t ctrlxfer_msg;
    L4_CtrlXferItem_t item;
    L4_MsgTag_t t;

    if( rm ) {
	L4_Clear (&ctrlxfer_msg);

	L4_GPRegsCtrlXferItem_t gpr_item;
	gpr_item = L4_GPRegsCtrlXferItem();
	L4_GPRegsCtrlXferItemSetReg(&gpr_item, L4_CTRLXFER_GPREGS_EIP, start_ip);
	L4_GPRegsCtrlXferItemSetReg(&gpr_item, L4_CTRLXFER_GPREGS_ESP, start_sp);
	// if the disk is larger than 3 MB, assume it is a hard disk
	if( get_vm()->ramdisk_size >= MB(3) ) {
	    L4_GPRegsCtrlXferItemSetReg(&gpr_item, L4_CTRLXFER_GPREGS_EDX, 0x80);
	} else {
	    L4_GPRegsCtrlXferItemSetReg(&gpr_item, L4_CTRLXFER_GPREGS_EDX, 0x00);
	}
	L4_Append(&ctrlxfer_msg, &gpr_item);

	L4_SegmentCtrlXferItem_t cs_item;
	cs_item = L4_SegmentCtrlXferItem(L4_CTRLXFER_CSREGS_ID);
	L4_SegmentCtrlXferItemSetReg(&cs_item, L4_CTRLXFER_CSREGS_CS, start_cs);
	L4_SegmentCtrlXferItemSetReg(&cs_item, L4_CTRLXFER_CSREGS_CS_BASE, start_cs << 4);
	L4_Append(&ctrlxfer_msg, &cs_item);

	L4_SegmentCtrlXferItem_t ss_item;
	ss_item = L4_SegmentCtrlXferItem(L4_CTRLXFER_SSREGS_ID);
	L4_SegmentCtrlXferItemSetReg(&ss_item, L4_CTRLXFER_CSREGS_CS, start_ss);
	L4_SegmentCtrlXferItemSetReg(&ss_item, L4_CTRLXFER_CSREGS_CS_BASE, start_ss << 4);
	L4_Append(&ctrlxfer_msg, &ss_item);

	L4_Load (&ctrlxfer_msg);

    } else { // protected mode
#if 0
	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetMultiple;

	item.mul.row = 3;
	item.mul.mask = 0x00ff;
	L4_LoadMR( 1, item.raw );
	L4_LoadMR( 2, 1 << 3 );
	L4_LoadMR( 3, 2 << 3 );
	L4_LoadMR( 4, 0 );
	L4_LoadMR( 5, 0 );
	L4_LoadMR( 6, 0 );
	L4_LoadMR( 7, 0 );
	L4_LoadMR( 8, 0 );
	L4_LoadMR( 9, 0 );

	item.mul.row = 4;
	item.mul.mask = 0x0003;
	L4_LoadMR( 10, item.raw );
	L4_LoadMR( 11, ~0UL );
	L4_LoadMR( 12, ~0UL );

	item.mul.row = 5;
	item.mul.mask = 0x00ff;
	L4_LoadMR( 13, item.raw );
	L4_LoadMR( 14, 0x0c099 );
	L4_LoadMR( 15, 0x0c093 );
	L4_LoadMR( 16, 0x10000 );
	L4_LoadMR( 17, 0x10000 );
	L4_LoadMR( 18, 0x10000 );
	L4_LoadMR( 19, 0x10000 );
	L4_LoadMR( 20, 0x0808b );
	L4_LoadMR( 21, 0x18003 );

	item.mul.row = 6;
	item.mul.mask = 0x0003;
	L4_LoadMR( 22, item.raw );
	L4_LoadMR( 23, 0 );
	L4_LoadMR( 24, 0 );

	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetRegister;

	item.reg.index = L4_VcpuReg_cr0;
	L4_LoadMR( 25, item.raw );
	L4_LoadMR( 26, 0x00000031 );

	item.reg.index = L4_VcpuReg_eip;
	L4_LoadMR( 27, item.raw );
	L4_LoadMR( 28, start_ip );

	item.reg.index = L4_VcpuReg_esi;
	L4_LoadMR( 29, item.raw );
	L4_LoadMR( 30, 0x9022 );

	t.raw = 0;
	t.X.label = L4_LABEL_VFAULT_REPLY << 4;
	t.X.u = 30;
	L4_Set_MsgTag( t );
#endif
    }
}

bool thread_info_t::process_vfault_message()
{
    L4_MsgTag_t	tag = L4_MsgTag();
    word_t vector, irq;
    intlogic_t &intlogic   = get_intlogic();
    L4_Word_t reg;
    L4_VirtFaultOperand_t operand;
    L4_VirtFaultIO_t io;
    L4_Word_t value;
    printf("Received vfault message with tag %x\n",L4_Label(tag));

    // store GPRegs
    L4_StoreMRs(L4_UntypedWords(tag)+1, L4_CTRLXFER_GPREGS_SIZE+1, (L4_Word_t*)&gpr_item);
    ASSERT((gpr_item.item.__type) == 0x06 ); // ctrlxfer item
    ASSERT((gpr_item.item.id) == 0); // gpregs

    switch( L4_Label( tag )) {
	case (L4_LABEL_REGISTER_FAULT << 4) | 0x2 | 0x8:
	// TODO implement debug registers in kernel and remove this
	case (L4_LABEL_REGISTER_FAULT << 4) | 0x2:
	    L4_StoreMR(3, &reg);
	    L4_StoreMR(4, &operand.raw);
	    L4_StoreMR(5, &value);
	    return this->handle_register_write(reg, operand, value);

	case (L4_LABEL_REGISTER_FAULT << 4) | 0x4 | 0x8:
	    return this->handle_register_read();

	case L4_LABEL_INSTRUCTION_FAULT << 4:
	    L4_StoreMR(3, &reg);
	    return this->handle_instruction(reg);

	case (L4_LABEL_EXCEPTION_FAULT << 4):
	    return this->handle_exception();

    //	case L4_LABEL_EXCEPTION_FAULT << 4:
    //	    return this->handle_bios_call();

	case (L4_LABEL_IO_FAULT << 4) | 0x2:
	    L4_StoreMR( 3, &io.raw );
	    L4_StoreMR( 4, &operand.raw );
	    L4_StoreMR( 5, &value );
	    return this->handle_io_write(io, operand, value);

	case (L4_LABEL_IO_FAULT << 4) | 0x4:
	    return this->handle_io_read();

	case (L4_LABEL_MSR_FAULT << 4) | 0x2 | 0x8:
	    return this->handle_msr_write();

	case (L4_LABEL_MSR_FAULT << 4) | 0x4 | 0x8:
	    return this->handle_msr_read();

	case (L4_LABEL_MSR_FAULT << 4) | 0x2:
	    return this->handle_unknown_msr_write();

	case (L4_LABEL_MSR_FAULT << 4) | 0x4:
	    return this->handle_unknown_msr_read();

	case L4_LABEL_DELAYED_FAULT << 4:	
	    this->wait_for_interrupt_window_exit = false;
	    if( intlogic.pending_vector( vector, irq ) ) 
	    {
		//L4_TBUF_RECORD_EVENT(12, "IL delayed fault, deliver irq %d", irq);
		dprintf(irq_dbg_level(irq), "IL df, irq %d\n", irq);
		this->handle_interrupt(vector, irq);
	    }
	    return true; 
	default:
	    printf( "unhandled message tag %x from %t\n", tag.raw, tid);
	    DEBUGGER_ENTER("monitor: unhandled message");
	    return false;
    }
}

INLINE L4_Word_t thread_info_t::get_ip()
{
    L4_Word_t ip;

    L4_StoreMR( 1, &ip );

    return ip;
}

INLINE L4_Word_t thread_info_t::get_instr_len()
{
    L4_Word_t instr_len;

    L4_StoreMR( 2, &instr_len );

    return instr_len;
}

INLINE L4_Word_t thread_info_t::request_items()
{
    L4_Word_t dummy, old_control;
    L4_ThreadId_t mtid;
    L4_ExchangeRegisters (this->tid, (1<<12), 0, 0 , 0, 0, L4_nilthread,
			  &old_control, &dummy, &dummy, &dummy, &dummy, &mtid);
    return 0;
}

bool thread_info_t::handle_real_mode_fault()
{
    //    vmcs_t *vmcs		= this->get_vmcs ();
    //    x86_exceptionframe_t *gprs	= this->get_gprs ();
    word_t cs_base;//		= vmcs->gs.cs_base;
    word_t guest_ip = this->get_ip(); //		= vmcs->gs.rip;
    u8_t *linear_ip;//		= (u8_t *) (cs_base + guest_ip);
    word_t data_size		= 16;
    word_t addr_size		= 16;
    u8_t modrm;
    word_t operand_addr		= -1UL;
    word_t operand_data		= 0UL;
    bool rep			= false;
    bool seg_ovr		= false;
    word_t seg_ovr_base         = 0;
    L4_SegmentCtrlXferItem_t cs_item, ds_item, es_item, gs_item;
    L4_MsgTag_t	tag = L4_MsgTag();
    L4_Msg_t ctrlxfer_msg;
    L4_VirtFaultException_t except;
    L4_VirtFaultOperand_t operand;
    L4_VirtFaultIO_t io;
    L4_Word_t mem_addr;
    //virt_exception_t except;
    //virt_msg_operand_t operand;
    //virt_msg_io_t io;

    // This cannot happen unless the monitor did something wrong.
    //    if (!get_current_space ()->is_user_area (linear_ip))
    //	return false;
    L4_StoreMR( 3, &except.raw );

    // Request additional VCPU state (cs,ds,es,gs)
    cs_item = L4_SegmentCtrlXferItem(4);
    cs_item.item.mask = 0xf;
    cs_item.item.C=1;
    ds_item = L4_SegmentCtrlXferItem(6);
    ds_item.item.mask = 0xf;
    ds_item.item.C=1;
    es_item = L4_SegmentCtrlXferItem(7);
    es_item.item.mask = 0xf;
    es_item.item.C=1;
    gs_item = L4_SegmentCtrlXferItem(9);
    gs_item.item.mask = 0xf;
    L4_Clear(&ctrlxfer_msg);
    L4_Append(&ctrlxfer_msg, &cs_item);
    L4_Append(&ctrlxfer_msg, &ds_item);
    L4_Append(&ctrlxfer_msg, &es_item);
    L4_Append(&ctrlxfer_msg, &gs_item);
    L4_Load(&ctrlxfer_msg);
    // Read new items via exregs
    request_items();

    tag = L4_MsgTag();
    L4_StoreMRs(L4_UntypedWords(tag)+1, 5, (L4_Word_t*)&cs_item);
    L4_StoreMRs(L4_UntypedWords(tag)+6, 5, (L4_Word_t*)&ds_item);
    L4_StoreMRs(L4_UntypedWords(tag)+11, 5, (L4_Word_t*)&es_item);
    L4_StoreMRs(L4_UntypedWords(tag)+16, 5, (L4_Word_t*)&gs_item);
    ASSERT(cs_item.item.id == 4);
    ASSERT(ds_item.item.id == 6);
    ASSERT(es_item.item.id == 7);
    ASSERT(gs_item.item.id == 9);

    linear_ip = (u8_t*) (guest_ip + cs_item.regs.base);
    printf("Linear fault IP: %p\n", linear_ip);

    L4_KDB_Enter("handle_real_mode_fault");

    switch (except.X.type)
    {
    case L4_ExceptionGP:		// General protection fault.
    {
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
		seg_ovr_base = es_item.regs.base;//vmcs->gs.es_base;
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
			    operand_addr = gpr_item.gpregs.eax;//gprs->eax;
			    break;
			case 0x1:
			    operand_addr = gpr_item.gpregs.ecx;//gprs->ecx;
			    break;
			case 0x2:
			    operand_addr = gpr_item.gpregs.edx;//gprs->edx;
			    break;
			case 0x3:
			    operand_addr = gpr_item.gpregs.ebx;//gprs->ebx;
			    break;
			case 0x6:
			    operand_addr = gpr_item.gpregs.esi;//gprs->esi;
			    break;
			case 0x7:
			    operand_addr = gpr_item.gpregs.edi;//gprs->edi;
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
			    operand_addr = gpr_item.gpregs.esi;//gprs->esi;
			    break;
			case 0x5:
			    operand_addr = gpr_item.gpregs.edi;//gprs->edi;
			    break;
			case 0x7:
			    operand_addr = gpr_item.gpregs.ebx;//gprs->ebx;
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
			operand_data = gpr_item.gpregs.eax;//gprs->eax;
			break;
		    case 0x1:
			operand_data = gpr_item.gpregs.ecx;//gprs->ecx;
			break;
		    case 0x2:
			operand_data = gpr_item.gpregs.edx;//gprs->edx;
			break;
		    case 0x3:
			operand_data = gpr_item.gpregs.ebx;//gprs->ebx;
			break;
		    case 0x4:
			operand_data = gpr_item.gpregs.esp;//vmcs->gs.rsp;
			L4_KDB_Enter("recheck");
			break;
		    case 0x5:
			operand_data = gpr_item.gpregs.ebp;//gprs->ebp;
			break;
		    case 0x6:
			operand_data = gpr_item.gpregs.esi;//gprs->esi;
			break;
		    case 0x7:
			operand_data = gpr_item.gpregs.edi;//gprs->edi;
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
		operand.reg.index	= L4_VcpuReg_eax + (modrm & 0x7);


		//		    msg_handler->send_register_fault
		//	((virt_vcpu_t::vcpu_reg_e)
		//	 (virt_vcpu_t::r_cr0 + ((modrm >> 3) & 0x7)),
		//	 *(linear_ip + 1) == 0x22,
		//	 &operand);
		if( *(linear_ip+1) == 0x22) // register write
		    return handle_register_write( (L4_VcpuReg_cr0 + ((modrm >> 3) & 0x7)),
						  operand, gpr_item.gpregs.reg[(modrm & 0x7)]);
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
	    io.raw			= 0;
	    io.X.rep		= rep;
	    io.X.port		= gpr_item.gpregs.edx & 0xffff;
	    io.X.access_size	= data_size;
		
	    operand.raw		= 0;
	    operand.X.type		= L4_OperandMemory;
		
		
	    if (seg_ovr)
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (seg_ovr_base + (gpr_item.gpregs.esi & 0xffff))
		    : (seg_ovr_base + (gpr_item.gpregs.edi & 0xffff));
	    else
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (ds_item.regs.base + (gpr_item.gpregs.esi & 0xffff))
		    : (es_item.regs.base + (gpr_item.gpregs.edi & 0xffff));
		
	    if(*linear_ip >= 0x6e) // write
		return handle_io_write(io, operand, mem_addr);
	    else
		printf("todo\n");
	    return true;
	case 0xcc:				// int 3.
	case 0xcd:				// int n.
	    /*		except.raw	= 0;
	      except.vector	= *linear_ip == 0xcc ? 3 : *(linear_ip + 1);
	      except.type	= virt_exception_t::e_sw_int;
	      except.valid	= true;*/

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
	    operand.X.type		= L4_OperandRegister;
	    operand.reg.index	= L4_VcpuReg_eax;

	    if(*linear_ip >= 0xe6) // write
		return handle_io_write(io, operand, gpr_item.gpregs.eax);
	    else
		printf("todo\n");
	    return true;
	case 0xec:				// inb dx.
	case 0xee:				// outb dx.
	    data_size = 8;
	    // fall through
	case 0xed:				// in dx.
	case 0xef:				// out dx.
	    io.raw			= 0;
	    io.X.rep		= rep;
	    io.X.port		= gpr_item.gpregs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type		= L4_OperandRegister;
	    operand.reg.index	= L4_VcpuReg_eax;

	    if(*linear_ip >= 0xee) // write
		return handle_io_write(io, operand, gpr_item.gpregs.eax);
	    else
		printf("todo\n");
	    return true;
	case 0xf4:				// hlt
	    return handle_instruction(L4_VcpuIns_hlt);
	}
    }
    }

    return false;
}


bool thread_info_t::handle_register_write(L4_Word_t reg, L4_VirtFaultOperand_t operand, L4_Word_t value)
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    //    L4_Word_t reg;
    //    L4_VirtFaultOperand_t operand;
    //    L4_Word_t value;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_register_write");
    L4_StoreMR( 3, &reg );
    L4_StoreMR( 4, &operand.raw );

    if( operand.X.type == L4_OperandMemory ) {
	DEBUGGER_ENTER("monitor: memory source operands unhandled");
	return false;
    }

    L4_StoreMR( 5, &value );

    if( debug_vfault )
	printf( "%x: write to register %x val %x\n", ip, reg, value); 


    if(reg == L4_VcpuReg_cr0)
	cr0.x.raw = value;
    if(reg == L4_VcpuReg_cr3)
	cr3.x.raw = value;

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;

    item.reg.index = reg;
    L4_LoadMR( 1, item.raw );
    L4_LoadMR( 2, value );

    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( 3, item.raw );
    L4_LoadMR( 4, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = 4;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_register_read()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t reg;
    L4_VirtFaultOperand_t operand;
    L4_Word_t value;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_register_read");
    L4_StoreMR( 3, &reg );
    L4_StoreMR( 4, &operand.raw );

    if( operand.X.type != L4_OperandRegister ) {
	DEBUGGER_ENTER("monitor: non-register target operands unhandled");
	return false;
    }

    L4_StoreMR( 5, &value );

    if( debug_vfault )
	printf( "%x: read from register %x val %x\n", ip, reg, value); 

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;

    item.reg.index = operand.reg.index;
    L4_LoadMR( 1, item.raw );
    L4_LoadMR( 2, value );

    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( 3, item.raw );
    L4_LoadMR( 4, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = 4;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_instruction(L4_Word_t instruction)
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    //    L4_Word_t instruction;
    L4_Word_t value;
    u64_t value64;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    frame_t frame;
    L4_KDB_Enter("handle_instruction");
    L4_StoreMR( 3, &instruction );

    if( debug_vfault )
	printf("%x: instruction %x\n", ip, instruction);

    switch( instruction ) {
	case L4_VcpuIns_cpuid:
	    L4_StoreMR( 5, &value );
	    frame.x.fields.eax = value;

	    handle_cpuid( &frame );

	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetMultiple;
	    item.mul.row = 0;
	    item.mul.mask = 0x000f;
	    L4_LoadMR( 1, item.raw );
	    L4_LoadMR( 2, frame.x.fields.eax );
	    L4_LoadMR( 3, frame.x.fields.ecx );
	    L4_LoadMR( 4, frame.x.fields.edx );
	    L4_LoadMR( 5, frame.x.fields.ebx );

	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_eip;
	    L4_LoadMR( 6, item.raw );
	    L4_LoadMR( 7, next_ip );

	    tag.raw = 0;
	    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
	    tag.X.u = 7;
	    L4_Set_MsgTag( tag );

	    return true;

	case L4_VcpuIns_hlt:
	    // wait until next interrupt arrives
	    this->resume_ip = next_ip;
	    this->state = thread_state_waiting_for_interrupt;

	    return true;

	case L4_VcpuIns_invlpg:
	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_eip;
	    L4_LoadMR( 1, item.raw );
	    L4_LoadMR( 2, next_ip );

	    tag.raw = 0;
	    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
	    tag.X.u = 2;
	    L4_Set_MsgTag( tag );

	    return true;

	case L4_VcpuIns_rdtsc:
	    value64 = ia32_rdtsc();

	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetMultiple;
	    item.mul.row = 0;
	    item.mul.mask = 0x0005;
	    L4_LoadMR( 1, item.raw );
	    L4_LoadMR( 2, value64 );
	    L4_LoadMR( 3, value64 >> 32 );

	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_eip;
	    L4_LoadMR( 4, item.raw );
	    L4_LoadMR( 5, next_ip );

	    tag.raw = 0;
	    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
	    tag.X.u = 5;
	    L4_Set_MsgTag( tag );

	    return true;

	case L4_VcpuIns_monitor:
	case L4_VcpuIns_mwait:
	case L4_VcpuIns_pause:
	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_eip;
	    L4_LoadMR( 1, item.raw );
	    L4_LoadMR( 2, next_ip );

	    tag.raw = 0;
	    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
	    tag.X.u = 2;
	    L4_Set_MsgTag( tag );

	    return true;

	default:
	    printf("%x: unhandled instruction %x\n", ip, instruction);
	    DEBUGGER_ENTER("monitor: unhandled instruction");
	    return false;
    }
}

bool thread_info_t::handle_exception()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t instr_len = this->get_instr_len();
    L4_VirtFaultException_t except;
    L4_Word_t err_code = 0;
    L4_Word_t addr = 0;

    //  If guest is in real mode do special fault handling
    if( !cr0.protected_mode_enabled() )
    {
	if(this->handle_real_mode_fault())
	    return true;
    }
     
    L4_KDB_Enter("handle_exception");
    L4_StoreMR( 3, &except.raw );
    if( except.X.has_err_code ) {
	L4_StoreMR( 4, &err_code );
	// page fault: store address
	if( except.X.vector == 14 ) {
	    L4_StoreMR( 5, &addr );
	}
    }

    if( debug_vfault )
	printf( "%x: exception %x %x %x\n", ip, except.raw, err_code, addr); 

    L4_Word_t mrs = 0;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;

    item.raw = 0;
    item.X.type = L4_VirtFaultReplyInject;
    L4_LoadMR( ++mrs, item.raw );
    L4_LoadMR( ++mrs, except.raw );
    L4_LoadMR( ++mrs, instr_len );
    if( except.X.has_err_code ) {
	L4_LoadMR( ++mrs, err_code );
    }

    if( except.X.vector == 14 ) {
	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetRegister;
	item.reg.index = L4_VcpuReg_cr2;
	L4_LoadMR( ++mrs, item.raw );
	L4_LoadMR( ++mrs, addr );
    }

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_bios_call()
{
    L4_KDB_Enter("handle_bios_call");
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_VirtFaultException_t except;

    L4_StoreMR( 3, &except.raw );

    if( except.X.type != L4_ExceptionInt || except.X.has_err_code ) {
	printf("%x: exception %x in real mode\n", ip, except.raw);
	DEBUGGER_ENTER("monitor: real mode exception");
	return false;
    }
#if defined(CONFIG_VBIOS)
    return vm8086_interrupt_emulation(except.X.vector, false);
#else
		
    return false;
#endif /* CONFIG_VBIOS */
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


/* Translate a guest virtual address to a guest physical address by looking
 * up the entry in the guest pagetable
 * returns 0 if not present */
L4_Word_t thread_info_t::gva_to_gpa( L4_Word_t vaddr , L4_Word_t &attr)
{
    pgent_t *pdir = (pgent_t*)(cr3.get_pdir_addr());
    pdir += pgent_t::get_pdir_idx(vaddr);

    if(!pdir->is_valid()) {
	printf( "Invalid pdir entry\n");
	return 0;
    }
    if(pdir->is_superpage()) {
	attr = (pdir->get_raw() & 0xfff);
	return (pdir->get_address() | (vaddr & ~(SUPERPAGE_MASK)));
    }

    pgent_t *ptab = (pgent_t*)pdir->get_address();
    ptab += pgent_t::get_ptab_idx(vaddr);

    if(!ptab->is_valid()) {
	printf( "Invalid ptab entry\n");
	return 0;
    }
    attr = (ptab->get_raw() & 0xfff);
    return (ptab->get_address() | (vaddr & ~(PAGE_MASK)));
}


bool thread_info_t::handle_io_write(L4_VirtFaultIO_t io, L4_VirtFaultOperand_t operand, L4_Word_t value)
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    //    L4_VirtFaultIO_t io;
    //    L4_VirtFaultOperand_t operand;
    L4_Word_t mem_addr;
    L4_Word_t paddr;
    L4_Word_t ecx = 1;
    L4_Word_t esi, eflags;
    L4_Word_t mrs = 0;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_io_write");
    //    L4_StoreMR( 3, &io.raw );
    //    L4_StoreMR( 4, &operand.raw );
    //    L4_StoreMR( 5, &value );

    if( debug_io && io.X.port != 0xcf8 && io.X.port != 0x3f8 )
	printf("%x: write to io port %x value %x\n", ip, io.X.port, value);

#if defined(CONFIG_VBIOS)
    if( io.X.port >= 0x400 && io.X.port <= 0x403 ) { // ROMBIOS debug ports
	printf( (char)value;
    }
    else if( io.X.port == 0xe9 )  // VGABIOS debug port
	printf( (char)value;     
    else 
#endif
	{
	switch( operand.X.type ) {

	case L4_OperandRegister:
	    //	    L4_StoreMR( 5, &value );
	    if( !portio_write( io.X.port, value & ((2 << io.X.access_size-1) - 1),
			       io.X.access_size ) ) {
		// TODO inject exception?
		printf("%x: write to io port %x value %x failed\n", ip, io.X.port, value);
		//DEBUGGER_ENTER("monitor: io write failed");
		//return false;
	    }
	    break;

	case L4_OperandMemory:
	    if(cr0.protected_mode_enabled())
		DEBUGGER_ENTER("String IO write with pe mode");

	    //L4_StoreMR( 5, &mem_addr );
	    mem_addr=value;
	    if( io.X.rep )
		ecx = gpr_item.gpregs.ecx;

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

	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_esi;
	    L4_LoadMR( ++mrs, item.raw );
	    L4_LoadMR( ++mrs, esi + 2*(ecx & 0xffff) );
	    break;

	err_io:
	    printf("%x: string write to io port %x value %x failed\n", ip, io.X.port, value);
	    break;

	default:
	    DEBUGGER_ENTER("monitor: unhandled io write");
	}
    }

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( ++mrs, item.raw );
    L4_LoadMR( ++mrs, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_io_read()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t mem_addr;
    L4_Word_t paddr;
    L4_Word_t mrs = 0;
    L4_Word_t edi, eflags;
    L4_Word_t count, size = 0;
    L4_VirtFaultIO_t io;
    L4_VirtFaultOperand_t operand;
    L4_Word_t reg_value;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_ThreadId_t vtid;
    word_t value = 0;
    L4_KDB_Enter("handle_io_read");
    L4_StoreMR( 3, &io.raw );
    L4_StoreMR( 4, &operand.raw );

    if( debug_io && io.X.port != 0xcfc && io.X.port != 0x3fd && io.X.port != 0x64 )
	printf("%x: read from io port %x\n", ip, io.X.port);

    switch( operand.X.type ) {

    case L4_OperandRegister:
	L4_StoreMR( 5, &reg_value );
	if( !portio_read( io.X.port, value, io.X.access_size ) ) {
	    // TODO inject exception?
	    printf("%x: read from io port %x failed \n", ip, io.X.port);
	}
	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetRegister;

	item.reg.index = operand.reg.index;
	L4_LoadMR( ++mrs, item.raw );
	L4_LoadMR( ++mrs, (reg_value & ~((2 << io.X.access_size-1) - 1)) | value );

	break;

    case L4_OperandMemory:

	L4_StoreMR( 5, &mem_addr );
	if( io.X.rep ) {
	    L4_StoreMR( 6, &count ); // ecx
	    L4_StoreMR( 7, &edi );
	    L4_StoreMR( 8, &eflags );
	    if( cr0.real_mode() )
		count &= 0xFFFF;
	} else {
	    L4_StoreMR( 6, &edi );
	    L4_StoreMR( 7, &eflags );
	    count = 1;
	}

	if( cr0.protected_mode_enabled() ) {

	    if(cr0.paging_enabled() ) {
		/* paging enabled, lookup paddr in pagetable */
		L4_Word_t attr;
		paddr = gva_to_gpa(mem_addr, attr);

		if(!paddr) {
		    L4_VirtFaultException_t pf_except;
		    L4_Word_t instr_len = this->get_instr_len();
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
		    item.X.type = L4_VirtFaultReplySetRegister;
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
	    else {
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

	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetRegister;
	item.reg.index = L4_VcpuReg_edi;
	L4_LoadMR( ++mrs, item.raw );
	L4_LoadMR( ++mrs, edi + size );
	break;

    err_io:
	// TODO: inject GP(0)?
	printf("%x: string read from io port %x failed \n", ip, io.X.port);
	break;

    default:
	DEBUGGER_ENTER("monitor: unhandled io read");
    }

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( ++mrs, item.raw );
    L4_LoadMR( ++mrs, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_msr_write()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t msr;
    L4_Word_t value1, value2;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_msr_write");
    L4_StoreMR( 3, &msr );
    L4_StoreMR( 4, &value1 );
    L4_StoreMR( 5, &value2 );

    if( debug_vfault )
	printf("%x: write to MSR %x value %x:%x ", ip, msr, value2, value1);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetMSR;
    L4_LoadMR( 1, item.raw );
    L4_LoadMR( 2, msr );
    L4_LoadMR( 3, value1 );
    L4_LoadMR( 4, value2 );

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( 5, item.raw );
    L4_LoadMR( 6, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = 6;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_msr_read()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t msr;
    L4_Word_t value1, value2;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_msr_read");
    L4_StoreMR( 3, &msr );
    L4_StoreMR( 4, &value1 );
    L4_StoreMR( 5, &value2 );

    if( debug_vfault )
	printf("%x: read from MSR %x value %x:%x ", ip, msr, value2, value1);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetMultiple;
    item.mul.row = 0;
    item.mul.mask = 0x0005;
    L4_LoadMR( 1, item.raw );
    L4_LoadMR( 2, value1 );
    L4_LoadMR( 3, value2 );

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( 4, item.raw );
    L4_LoadMR( 5, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = 5;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_unknown_msr_write()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t msr;
    L4_Word_t value1, value2;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_unknown_msr_read");
    L4_StoreMR( 3, &msr );
    L4_StoreMR( 4, &value1 );
    L4_StoreMR( 5, &value2 );

    printf("%x: unhandled write to MSR %x value %x:%x ", ip, msr, value2, value1);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( 1, item.raw );
    L4_LoadMR( 2, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = 2;
    L4_Set_MsgTag( tag );

    return true;
}

bool thread_info_t::handle_unknown_msr_read()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t msr;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_unknown_msr_read");
    L4_StoreMR( 3, &msr );

    printf("%x: unhandled read from MSR %x", ip, msr);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetMultiple;
    item.mul.row = 0;
    item.mul.mask = 0x0005;
    L4_LoadMR( 1, item.raw );

    switch (msr)
    {
	case 0x1a0:			// IA32_MISC_ENABLE
	    L4_LoadMR( 2, 0x00000c00 );
	    L4_LoadMR( 3, 0 );
	    break;

	default:
	    L4_LoadMR( 2, 0 );
	    L4_LoadMR( 3, 0 );
    }

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( 4, item.raw );
    L4_LoadMR( 5, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = 5;
    L4_Set_MsgTag( tag );

    return true;
}


typedef struct
{
    u16_t ip;
    u16_t cs;
} ia32_ive_t;

bool thread_info_t::vm8086_interrupt_emulation(word_t vector, bool hw)
{
    L4_Word_t next_ip = this->get_ip() + this->get_instr_len();
    L4_Word_t mrs = 0;
    L4_MsgTag_t tag;
    L4_VirtFaultReplyItem_t item;
    L4_ThreadId_t vtid;
    L4_Word_t sp, ip, eflags, cs, ss;
    L4_Word_t stack_addr;
    u16_t *stack;
    ia32_ive_t *int_vector;

    // Get SP 
    item.raw = 0;
    item.X.type = L4_VirtFaultReplyGetRegister;
    item.reg.index = L4_VcpuReg_esp;
    L4_LoadMR( ++mrs, item.raw);
    // Get eflags and ip
    item.raw = 0;
    item.X.type = L4_VirtFaultReplyGetMultiple;
    item.mul.row = 1;
    item.mul.mask = 0xC000;
    L4_LoadMR( ++mrs, item.raw);
    // Get cs and ss
    item.raw = 0;
    item.X.type = L4_VirtFaultReplyGetMultiple;
    item.mul.row = 3;
    item.mul.mask = 0x21;
    L4_LoadMR( ++mrs, item.raw);

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag(tag);

    tag = L4_ReplyWait(this->tid, &vtid);

    ASSERT( L4_Label(tag) == (L4_LABEL_VIRT_NORESUME << 4));
    ASSERT(vtid == this->tid);
    L4_StoreMR( 1, &sp);
    L4_StoreMR( 2, &eflags);
    L4_StoreMR( 3, &ip);
    L4_StoreMR( 4, &cs);
    L4_StoreMR( 5, &ss);

    if(!(eflags & 0x200) && hw) {
	printf( "WARNING: hw interrupt injection with if flag disabled !!!\n");
	mrs = 0;
	goto erply;
    }

    //    printf("\nReceived int vector %x\n", vector);
    //    printf("Got: sp:%x, efl:%x, ip:%x, cs:%x, ss:%x\n", sp, eflags, ip, cs, ss);
    stack_addr = (ss << 4) + (sp & 0xffff);
    stack = (u16_t*) get_vcpu().get_map_addr( stack_addr );
    // push eflags, cs and ip onto the stack
    *(--stack) = eflags & 0xffff;
    *(--stack) = cs & 0xffff;
	if(hw)
	    *(--stack) = ip & 0xffff;
	else
	    *(--stack) = next_ip & 0xffff;

    if( sp-6 < 0 )
	DEBUGGER_ENTER("stackpointer below segment");
    
    // get entry in interrupt vector table from guest
    int_vector = (ia32_ive_t*) get_vcpu().get_map_addr( vector*4 );
    if(debug_irq_inject)
	printf("Ii: %x (%c), entry: %x, %x at: %x\n", vector, hw ? 'h' : 's',  int_vector->ip, int_vector->cs, (cs << 4) + ip);

    eflags &= ~0x200;
    mrs = 0;

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_esp;
    L4_LoadMR( ++mrs, item.raw),
    L4_LoadMR( ++mrs, sp-6);
    
    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eip;
    L4_LoadMR( ++mrs, item.raw);
    L4_LoadMR( ++mrs, int_vector->ip);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_eflags;
    L4_LoadMR( ++mrs, item.raw);
    L4_LoadMR( ++mrs, eflags);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_cs;
    L4_LoadMR( ++mrs, item.raw);
    L4_LoadMR( ++mrs, int_vector->cs);

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetRegister;
    item.reg.index = L4_VcpuReg_cs_base;
    L4_LoadMR( ++mrs, item.raw);
    L4_LoadMR( ++mrs, (int_vector->cs << 4));
 erply:
    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag(tag);
    
    return true;
}


bool thread_info_t::handle_interrupt( L4_Word_t vector, L4_Word_t irq, bool set_ip)
{
    L4_VirtFaultException_t except;
    L4_VirtFaultReplyItem_t item;
    L4_Word_t mrs = 0;
    L4_MsgTag_t tag;
    L4_KDB_Enter("handle_interrupt");
#if defined(CONFIG_VBIOS)
    if( cr0.real_mode() ) {
	// Real mode, emulate interrupt injection
	return vm8086_interrupt_emulation(vector, true);
    }
#endif

    except.raw = 0;
    except.X.vector = vector;
    except.X.type = L4_ExceptionExtInt;
    except.X.valid = 1;

    if( set_ip ) {
	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetRegister;
	item.reg.index = L4_VcpuReg_eip;
	L4_LoadMR( ++mrs, item.raw );
	L4_LoadMR( ++mrs, this->resume_ip );
    }

    item.raw = 0;
    item.X.type = L4_VirtFaultReplyInject;
    L4_LoadMR( ++mrs, item.raw );
    L4_LoadMR( ++mrs, except.raw );
    L4_LoadMR( ++mrs, 0 );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag( tag );

    return true;
}

// signal to vcpu that we would like to deliver an interrupt
bool thread_info_t::deliver_interrupt(L4_Word_t vector, L4_Word_t irq)
{
    intlogic_t &intlogic   = get_intlogic();
    
    if( this->state == thread_state_waiting_for_interrupt ) {
	ASSERT( !this->wait_for_interrupt_window_exit );

	//    L4_TBUF_RECORD_EVENT(12, "IL deliver irq immediately %d", irq);
	dprintf(irq_dbg_level(irq), "INTLOGIC deliver irq immediately %d\n", irq);
    
	this->handle_interrupt( vector, irq, true );
	this->state = thread_state_running;
	
	return true;
    } 
    else 
    {
	// are we already waiting for an interrupt window exit
	if( this->wait_for_interrupt_window_exit )
	  return false;

	//   L4_TBUF_RECORD_EVENT(12, "IL delay irq via window exit %d", irq);
	dprintf(irq_dbg_level(irq), "INTLOGIC delay irq via window exit %d\n", irq);

	// inject interrupt request
	this->wait_for_interrupt_window_exit = true;
	
	ASSERT(tid != L4_nilthread);
	L4_ForceDelayedFault( tid );
	
	// no immediate delivery
	return false;
    }
}


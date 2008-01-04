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
	goto err_activate;
    }

        
    dprintf(debug_startup, "Main thread initialized TID %t VCPU %d\n", main_gtid, cpu_id);

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
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t t;

    if( rm ) {
	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetRegister;

	item.reg.index = L4_VcpuReg_eip;
	L4_LoadMR( 1, item.raw );
	L4_LoadMR( 2, start_ip );

	item.reg.index = L4_VcpuReg_edx;
	L4_LoadMR( 3, item.raw );
	// if the disk is larger than 3 MB, assume it is a hard disk
	if( get_vm()->ramdisk_size >= MB(3) ) {
	    L4_LoadMR( 4, 0x80 );
	} else {
	    L4_LoadMR( 4, 0x00 );
	}
	item.reg.index = L4_VcpuReg_esp;
	L4_LoadMR( 5, item.raw );
	L4_LoadMR( 6, start_sp);

	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetMultiple;
	item.mul.row = 3;
	item.mul.mask = 0x21;
	L4_LoadMR( 7, item.raw );
	L4_LoadMR( 8, start_cs );
	L4_LoadMR( 9, start_ss );

	item.raw = 0;
	item.X.type = L4_VirtFaultReplySetMultiple;
	item.mul.row = 6;
	item.mul.mask = 0x21;
	L4_LoadMR( 10, item.raw);
	L4_LoadMR( 11, start_cs << 4 );
	L4_LoadMR( 12, start_ss << 4 );

	t.raw = 0;
	t.X.label = L4_LABEL_VFAULT_REPLY << 4;
	t.X.u = 12;
	L4_Set_MsgTag( t );
    } else { // protected mode
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
    }
}

bool thread_info_t::process_vfault_message()
{
    L4_MsgTag_t	tag = L4_MsgTag();
    word_t vector, irq;
    intlogic_t &intlogic   = get_intlogic();

    switch( L4_Label( tag )) {
	case (L4_LABEL_REGISTER_FAULT << 4) | 0x2 | 0x8:
	// TODO implement debug registers in kernel and remove this
	case (L4_LABEL_REGISTER_FAULT << 4) | 0x2:
	    return this->handle_register_write();

	case (L4_LABEL_REGISTER_FAULT << 4) | 0x4 | 0x8:
	    return this->handle_register_read();

	case L4_LABEL_INSTRUCTION_FAULT << 4:
	    return this->handle_instruction();

	case (L4_LABEL_EXCEPTION_FAULT << 4) | 0x8:
	    return this->handle_exception();

	case L4_LABEL_EXCEPTION_FAULT << 4:
	    return this->handle_bios_call();

	case (L4_LABEL_IO_FAULT << 4) | 0x2:
	    return this->handle_io_write();

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

bool thread_info_t::handle_register_write()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t reg;
    L4_VirtFaultOperand_t operand;
    L4_Word_t value;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;

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

bool thread_info_t::handle_instruction()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_Word_t instruction;
    L4_Word_t value;
    u64_t value64;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    frame_t frame;

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

typedef struct {
    u8_t size;
    u8_t res0;
    u8_t sectors;
    u8_t res1;
    u16_t buffer_addr;
    u16_t buffer_seg;
    u64_t sector_start;
} ia32_dap_t;

typedef struct {
    u16_t size;
    u16_t flags;
    u32_t cylinders;
    u32_t heads;
    u32_t sectors;
    u64_t total_sectors;
    u16_t bytes_per_sector;
} ia32_drp_t;

typedef struct {
    u64_t base_addr;
    u64_t length;
    u32_t type;
} ia32_emm_t;

const word_t sector_size = 512;

bool thread_info_t::handle_bios_call()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_VirtFaultException_t except;
#if !defined(CONFIG_VBIOS)
    L4_Word_t eax, ecx, ebx, ds, es, esi, edi, eflags;
    L4_Word_t function;
    char c;
    word_t cylinder, sector;
    word_t cylinders;
    word_t dap_addr, buf_addr, drp_addr, emm_addr;
    word_t map_addr /*, map_bits, map_rwx*/;
    ia32_dap_t *dap;
    ia32_drp_t *drp;
    ia32_emm_t *emm;
    u8_t *ramdisk_start;
    word_t ramdisk_size;
    L4_Word_t mrs = 0;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;
    L4_Clock_t clock;
    L4_Word_t hours, minutes, seconds;
#endif
    L4_StoreMR( 3, &except.raw );

    if( except.X.type != L4_ExceptionInt || except.X.has_err_code ) {
	printf("%x: exception %x in real mode\n", ip, except.raw);
	DEBUGGER_ENTER("monitor: real mode exception");
	return false;
    }
#if defined(CONFIG_VBIOS)
    return vm8086_interrupt_emulation(except.X.vector, false);
#else

    L4_StoreMR( 4, &eax );
    L4_StoreMR( 12, &eflags );

    eflags |= 0x1;
    function = (eax >> 8) & 0xff;

    if( debug_vfault )
	printf( "%x: BIOS int %x function %x ", ip, except.X.vector, function);

    switch( except.X.vector ) {
	case 0x10:		// Text output.
	    switch( function ) {
		case 0x03:	// Get cursor.
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetMultiple;
		    item.mul.row = 0;
		    item.mul.mask = 0x0006;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );
		    L4_LoadMR( ++mrs, 0 );

		    break;

		case 0x09:	// Write character at cursor.
		case 0x0a:	// Write character at cursor.
		case 0x0e:	// Output character in TTY mode.
		    printf("%c", (char) (eax));

		    eflags &= ~0x1;
		    break;

		case 0x0f:	// Get video state.
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetMultiple;
		    item.mul.row = 0;
		    item.mul.mask = 0x0009;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );
		    L4_LoadMR( ++mrs, 0 );

		    break;

		case 0x00:	// Set video mode.
		case 0x01:	// Set cursor size.
		case 0x02:	// Set cursor position.
		case 0x12:	// Configure video.
		case 0x4f:	// VESA BIOS extensions.
		    break;
			
		default:
		    printf("%x: unhandled int 0x10 function %x\n", ip, function);
		    DEBUGGER_ENTER("monitor: unhandled BIOS function");
	    }

	    break;

	case 0x11:		// Basic system information.
	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_eax;
	    L4_LoadMR( ++mrs, item.raw );
	    L4_LoadMR( ++mrs, 0 );

	    break;

	case 0x12:		// Conventional memory size.
	    item.raw = 0;
	    item.X.type = L4_VirtFaultReplySetRegister;
	    item.reg.index = L4_VcpuReg_eax;
	    L4_LoadMR( ++mrs, item.raw );
	    L4_LoadMR( ++mrs, 640 );

	    eflags &= ~0x1;
	    break;

	case 0x13:		// Disk access.
	    ramdisk_start = (u8_t *) get_vm()->ramdisk_start;
	    if( !ramdisk_start ) {
		DEBUGGER_ENTER("monitor: no RAM disk");
		return false;
	    }
	    ramdisk_size = get_vm()->ramdisk_size;

	    switch( function ) {
		case 0x00:	// Reset disk drive.
		    eflags &= ~0x1;
		    break;

		case 0x02:	// Read in CHS mode.
		    L4_StoreMR( 5, &ecx );
		    L4_StoreMR( 9, &es );
		    L4_StoreMR( 7, &ebx );

		    // see below for disk geometry
		    cylinder = ((ecx & 0xff00) >> 8) | ((ecx & 0x00c0) << 2);
		    sector = ecx & 0x3f;

		    if( this->read_from_disk( ramdisk_start, ramdisk_size,
					      cylinder * 16 + sector - 1, eax & 0xff,
					      (es << 4) + (ebx & 0xffff) ) ) {
			eflags &= ~0x1;
		    }

		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_eax;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );

		    break;

		case 0x08:	// Query drive parameters.
		    // our simulated disk has a single head with 16 sectors
		    // maximum size is 8 MB, but hard disks will be read in LBA mode anyway
		    cylinders = (ramdisk_size - 1) / (16 * sector_size) + 1;
		    if( cylinders > 1024 ) {
			cylinders = 1024;
		    }
		    cylinders--;

		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetMultiple;
		    item.mul.row = 0;
		    item.mul.mask = 0x008f;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );
		    L4_LoadMR( ++mrs, ((cylinders & 0xff) << 8) | ((cylinders & 0x300) >> 2) | 16 );
		    L4_LoadMR( ++mrs, 1 );
		    L4_LoadMR( ++mrs, 0 );
		    // do we need to pass a drive parameter table for floppies?
		    L4_LoadMR( ++mrs, 0 );

		    eflags &= ~0x1;
		    break;

		case 0x41:	// Check extensions.
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetMultiple;
		    item.mul.row = 0;
		    item.mul.mask = 0x000b;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0x0100 );
		    L4_LoadMR( ++mrs, 0x0001 );
		    L4_LoadMR( ++mrs, 0xaa55 );

		    eflags &= ~0x1;
		    break;

		case 0x42:	// Read in LBA mode.
		    L4_StoreMR( 8, &ds );
		    L4_StoreMR( 10, &esi );

		    dap_addr = (ds << 4) + (esi & 0xffff);
		    map_addr = get_vcpu().get_map_addr( dap_addr );
#if 0
		    map_bits = SUPERPAGE_BITS;
		    // Increase mapping until it includes the end of the structure.
		    while ((dap_addr & ~((1 << map_bits) - 1)) + (1 << map_bits)
			   < dap_addr + sizeof( ia32_dap_t )) {
			map_bits++;
		    }
		    map_rwx = 0x4;
		    backend_handle_pagefault( dap_addr, ip, map_addr, map_bits, map_rwx );
		    map_addr |= dap_addr & ((1 << map_bits) - 1);
#endif
		    dap = (ia32_dap_t *) map_addr;
		    buf_addr = (dap->buffer_seg << 4) + dap->buffer_addr;

		    if( this->read_from_disk( ramdisk_start, ramdisk_size,
					      dap->sector_start, dap->sectors,
					      buf_addr ) ) {
			eflags &= ~0x1;
		    }

		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_eax;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );

		    break;

		case 0x48:	// Query physical drive parameters.
		    L4_StoreMR( 8, &ds );
		    L4_StoreMR( 10, &esi );

		    drp_addr = (ds << 4) + (esi & 0xffff);
		    map_addr = get_vcpu().get_map_addr( drp_addr );
#if 0
		    map_bits = SUPERPAGE_BITS;
		    while ((drp_addr & ~((1 << map_bits) - 1)) + (1 << map_bits)
			   < drp_addr + sizeof( ia32_drp_t )) {
			map_bits++;
		    }
		    map_rwx = 0x2;
		    backend_handle_pagefault( drp_addr, ip, map_addr, map_bits, map_rwx );
		    map_addr |= drp_addr & ((1 << map_bits) - 1);
#endif
		    drp = (ia32_drp_t *) map_addr;

		    drp->size = sizeof( ia32_drp_t );
		    drp->flags = 0;
		    drp->cylinders = drp->heads = 1;
		    drp->sectors = drp->total_sectors = ramdisk_size / sector_size;
		    drp->bytes_per_sector = sector_size;

		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_eax;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );

		    eflags &= ~0x1;
		    break;

		case 0x15:	// Read DASD Type.
		case 0x4b:	// Access CD-ROM.
		    break;

		default:
		    printf("%x: unhandled int 0x13 function %x\n", ip, function);
		    DEBUGGER_ENTER("monitor: unhandled BIOS function");
	    }

	    break;

	case 0x15:		// Extended features.
	    switch( function ) {
		case 0x24:	// A20 gate.
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_eax;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, 0 );

		    eflags &= ~0x1;
		    break;

		case 0x88:	// Extended memory size.
		    eax = (get_vm()->gphys_size - MB(1)) / KB(1);
		    if( eax > 0xffff ) {
			eax = 0xffff;
		    }

		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_eax;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, eax );

		    eflags &= ~0x1;
		    break;

		case 0xe8:	// EISA memory map.
		    switch( eax & 0xff )
		    {
			case 0x20:
			    L4_StoreMR( 7, &ebx );
			    L4_StoreMR( 9, &es );
			    L4_StoreMR( 11, &edi );

			    emm_addr = (es << 4) + (edi & 0xffff);
			    map_addr = get_vcpu().get_map_addr( emm_addr );
#if 0
			    map_bits = SUPERPAGE_BITS;
			    while ((emm_addr & ~((1 << map_bits) - 1)) + (1 << map_bits)
				   < emm_addr + sizeof( ia32_drp_t )) {
				map_bits++;
			    }
			    map_rwx = 0x2;
			    backend_handle_pagefault( emm_addr, ip, map_addr, map_bits, map_rwx );
			    map_addr |= emm_addr & ((1 << map_bits) - 1);
#endif
			    emm = (ia32_emm_t *) map_addr;

			    if( ebx == 0 ) {
				emm->base_addr = 0;
				emm->length = KB(640);
				ebx = 1;
			    } else {
				emm->base_addr = MB(1);
				emm->length = get_vm()->gphys_size - emm->base_addr;
				ebx = 0;
			    }
			    emm->type = 1;

			    item.raw = 0;
			    item.X.type = L4_VirtFaultReplySetMultiple;
			    item.mul.row = 0;
			    item.mul.mask = 0x000b;
			    L4_LoadMR( ++mrs, item.raw );
			    L4_LoadMR( ++mrs, 0x534D4150 );
			    L4_LoadMR( ++mrs, sizeof( ia32_emm_t ) );
			    L4_LoadMR( ++mrs, ebx );

			    eflags &= ~0x1;
			    break;
		    }

		    break;

		case 0x00:	// Get ROM config table.
		case 0x53:	// APM.
		case 0xc0:	// System configuration parameters.
		    break;

		default:
		    printf("%x: unhandled int 0x15 function %x\n", ip, function);
		    DEBUGGER_ENTER("monitor: unhandled BIOS function");
	    }

	    break;

	case 0x16:		// Keyboard input.
	    switch( function ) {
		case 0x00:	// Read character.
		    printf("%c", c);
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_eax;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, c );

		    eflags &= ~0x1;
		    break;

		case 0x03:	// Set typematic rate.
		    break;

		default:
		    printf("%x: unhandled int 0x16 function %x\n", ip, function);
		    DEBUGGER_ENTER("monitor: unhandled BIOS function");
	    }

	    break;
	    
	case 0x1a:             // Real time clock services
	    switch( function ) {
		case 0x02:     // read real time clock time

		    clock = L4_SystemClock();
		    clock.raw /= 1000000;  // get seconds
		    
		    seconds = (L4_Word_t) (clock.raw % 60);
		    clock.raw /= 60;
		    minutes = (L4_Word_t) (clock.raw % 60);
		    clock.raw /= 60;
		    hours = (L4_Word_t) (clock.raw % 24);
			
		    // convert to bcd
		    #define BIN2BCD(val)    ((((val)/10)<<4) + (val)%10)
		    		    
		    seconds = BIN2BCD( seconds );
		    minutes = BIN2BCD( minutes );
		    hours   = BIN2BCD( hours );
		    
		    item.raw = 0;
		    item.X.type = L4_VirtFaultReplySetRegister;
		    item.reg.index = L4_VcpuReg_ecx;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, (hours << 8) | minutes );

		    item.reg.index = L4_VcpuReg_edx;
		    L4_LoadMR( ++mrs, item.raw );
		    L4_LoadMR( ++mrs, (seconds << 8) );
		    
		    eflags &= ~0x1; // successful
		    break;
		
		default:
		    printf("%x: unhandled int 0x1a function %x\n", ip, function);
		    DEBUGGER_ENTER("monitor: unhandled BIOS function");
	    }
	    break;

	default:
	    printf("%x: unhandled int 0x%x function %x\n", ip, except.X.vector, function);
	    DEBUGGER_ENTER("monitor: unhandled BIOS interrupt");
    }

    item.raw = 0;
    item.X.type = L4_VirtFaultReplySetMultiple;
    item.mul.row = 1;
    item.mul.mask = 0xc000;
    L4_LoadMR( ++mrs, item.raw );
    L4_LoadMR( ++mrs, eflags );
    L4_LoadMR( ++mrs, next_ip );

    tag.raw = 0;
    tag.X.label = L4_LABEL_VFAULT_REPLY << 4;
    tag.X.u = mrs;
    L4_Set_MsgTag( tag );

    return true;
#endif /* CONFIG_VBIOS */
}

bool thread_info_t::read_from_disk( u8_t *ramdisk_start, word_t ramdisk_size, word_t sector_start, word_t sectors, word_t buf_addr )
{
    word_t byte_start = sector_start * sector_size;
    word_t bytes = sectors * sector_size;
    word_t map_addr /*, map_bits, map_rwx*/;
    void *buf;

    if( debug_ramdisk )
	printf( "read %d sectors starting at %d to %x\n", sectors, sector_start, buf_addr);

    map_addr = get_vcpu().get_map_addr( buf_addr );
#if 0
    map_bits = SUPERPAGE_BITS;
    while ((buf_addr & ~((1 << map_bits) - 1)) + (1 << map_bits)
	   < buf_addr + dap->sectors * sector_size) {
	map_bits++;
    }
    map_rwx = 0x2;
    backend_handle_pagefault( buf_addr, ip, map_addr, map_bits, map_rwx );
    map_addr |= buf_addr & ((1 << map_bits) - 1);
#endif
    buf = (void *) map_addr;

    if( byte_start + bytes > ramdisk_size ) {
	bytes = ramdisk_size - byte_start;
    }

    memcpy( buf, ramdisk_start + byte_start, bytes );

    return true;
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


bool thread_info_t::handle_io_write()
{
    L4_Word_t ip = this->get_ip();
    L4_Word_t next_ip = ip + this->get_instr_len();
    L4_VirtFaultIO_t io;
    L4_VirtFaultOperand_t operand;
    L4_Word_t value, mem_addr;
    L4_Word_t paddr;
    L4_Word_t ecx = 1;
    L4_Word_t esi, eflags;
    L4_Word_t mrs = 0;
    L4_VirtFaultReplyItem_t item;
    L4_MsgTag_t tag;

    L4_StoreMR( 3, &io.raw );
    L4_StoreMR( 4, &operand.raw );
    L4_StoreMR( 5, &value );

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
	    L4_StoreMR( 5, &value );
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

	    L4_StoreMR( 5, &mem_addr );
	    if( io.X.rep ) {
		L4_StoreMR( 6, &ecx );
		L4_StoreMR( 7, &esi );
		L4_StoreMR( 8, &eflags );
	    } else {
		L4_StoreMR( 6, &esi);
		L4_StoreMR( 7, &eflags);
	    }

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


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
#include INC_WEDGE(hvm_vmx.h)

void mr_save_t::load_startup_reply(word_t ip, word_t sp, word_t cs, word_t ss, bool real_mode)
{
    tag = startup_reply_tag();
    
    if( real_mode ) 
    {
	printf("untested switch real mode");
	
	gpregs_item.regs.eip = ip;
	gpregs_item.regs.esp = sp;
	gpregs_item.regs.eflags |= (X86_FLAGS_VM | X86_FLAGS_IOPL(3));
	
	// if the disk is larger than 3 MB, assume it is a hard disk
	if( get_vm()->ramdisk_size >= MB(3) ) 
	    gpregs_item.regs.edx = 0x80;
	else 
	    gpregs_item.regs.edx = 0;
	
	
	//Segment registers (VM8086 mode)
	append_seg_item(L4_CTRLXFER_CSREGS_ID, cs, cs << 4, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_SSREGS_ID, ss, ss << 4, 0xffff, 0xf4);
	append_seg_item(L4_CTRLXFER_DSREGS_ID, 0, 0, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_ESREGS_ID, 0, 0, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_FSREGS_ID, 0, 0, 0xffff, 0xf3);
	append_seg_item(L4_CTRLXFER_GSREGS_ID, 0, 0, 0xffff, 0xf3);
	
	hvm_vmx_segattr_t attr;
	attr.raw		= 0;
	attr.type		= 0x3;
	attr.p			= 1;
	// Mark tss as unused, to consistently generate #GPs
	attr.raw		&= ~(1<<16);
	printf("tr attr %x\n", attr.raw);
	append_seg_item(L4_CTRLXFER_TRREGS_ID, 0, 0, ~0, attr.raw);
	append_seg_item(L4_CTRLXFER_LDTRREGS_ID, 0, 0, ~0, 0x10082);
	
	DEBUGGER_ENTER("UNTESTED");				

    } 
    else 
    { 
	// protected mode
	gpregs_item.regs.eip = ip;
	gpregs_item.regs.esi = 0x9022;
	
	append_seg_item(L4_CTRLXFER_CSREGS_ID, 1<<3, 0, ~0, 0x0c099);
	append_seg_item(L4_CTRLXFER_SSREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_DSREGS_ID, 2<<3, 0, ~0, 0x0c093);
	append_seg_item(L4_CTRLXFER_ESREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_FSREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_GSREGS_ID, 0, 0, 0, 0x10000);
	append_seg_item(L4_CTRLXFER_TRREGS_ID, 0, 0, ~0, 0x0808b);
	append_seg_item(L4_CTRLXFER_LDTRREGS_ID, 0, 0, 0, 0x18003);
	
	append_cr_item(L4_CTRLXFER_CREGS_CR0, 0x00000031);
	append_cr_item(L4_CTRLXFER_CREGS_CR0_SHADOW, 0x00000031);
	append_cr_item(L4_CTRLXFER_CREGS_CR4_SHADOW, 0x000023d0);
    }
    dump(debug_startup+1);
    append_gpregs_item();
}

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
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask;
    L4_Word_t fault_mask;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

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
    
    if( get_vm()->guest_kernel_module == NULL ) 
	printf( "Starting in real mode\n");
    
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
    fault_mask = L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID);
    fault_id_mask = ((1ULL << L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_max))-1) & ~0x1c3;
    
    //by default, always send GPREGS
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(main_gtid);

    //for exp_nmi, also send exception info 
    fault_id_mask = L4_CTRLXFER_FAULT_ID_MASK(L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_exp_nmi));
    fault_mask |= L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_EXC_ID);
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(main_gtid);

    //for io faults, also send ds/es info 
    fault_id_mask = L4_CTRLXFER_FAULT_ID_MASK(L4_CTRLXFER_HVM_FAULT(hvm_vmx_reason_io));
    fault_mask = L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_GPREGS_ID) | 
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_DSREGS_ID) |
	L4_CTRLXFER_FAULT_MASK(L4_CTRLXFER_ESREGS_ID);
    L4_Clear(&ctrlxfer_msg);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask);
    L4_Load(&ctrlxfer_msg);
    L4_ConfCtrlXferItems(main_gtid);
    
    
    //Read execution control fields
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_PIN, 0);
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_CPU, 0);
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_EXC_BITMAP, 0);
    vcpu_mrs->load();
    L4_ReadCtrlXferItems(main_gtid);
    vcpu_mrs->store_excecctrl_item(1);
    printf("VCPU execution control pin %x cpu %x exb_bmp %x\n", 
	   vcpu_mrs->execctrl_item.regs.pin,vcpu_mrs->execctrl_item.regs.cpu, 
	   vcpu_mrs->execctrl_item.regs.exc_bitmap);
    
    
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
#if 0
bool handle_instruction(L4_Word_t instruction)
{
    u64_t value64;
    L4_Msg_t ctrlxfer_msg;
    frame_t frame;

    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    dprintf(debug_hvm_fault, "instruction fault %x ip %x\n", instruction, vcpu_mrs->gpregs_item.regs.eip);


    L4_Clear(&ctrlxfer_msg);

    switch( instruction ) {
	case L4_VcpuIns_hlt:
	    // wait until next interrupt arrives
	    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
	    return false;

	case L4_VcpuIns_invlpg:
	    DEBUGGER_ENTER("UNTESTED INVLPG");
	    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
	    vcpu_mrs->append_gpregs_item();

	    return true;
	case L4_VcpuIns_monitor:
	case L4_VcpuIns_mwait:
	case L4_VcpuIns_pause:
	    DEBUGGER_ENTER("UNTESTED MONITOR/MWAIT/PAUSE");
	    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
	    vcpu_mrs->append_gpregs_item();

	    return true;

	default:
	    printf("%x: unhandled instruction %x\n", vcpu_mrs->gpregs_item.regs.eip, instruction);
	    DEBUGGER_ENTER("monitor: unhandled instruction");
	    return false;
    }
}



bool handle_msr_write()
{
    L4_Word_t msr;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );

    dprintf(debug_hvm_fault, "write to MSR %x value %x:%x ip %x", 
	    msr, vcpu_mrs->gpregs_item.regs.edx, vcpu_mrs->gpregs_item.regs.eax, vcpu_mrs->gpregs_item.regs.eip);

    switch(msr)
    {
    case 0x174: // sysenter_cs
	vcpu_mrs->append_msr_item(L4_CTRLXFER_MSR_SYSENTER_CS, vcpu_mrs->gpregs_item.regs.eax);
	break;
    case 0x175: // sysenter_esp
	vcpu_mrs->append_msr_item(L4_CTRLXFER_MSR_SYSENTER_ESP, vcpu_mrs->gpregs_item.regs.eax);
	break;
    case 0x176: // sysenter_eip
	vcpu_mrs->append_msr_item(L4_CTRLXFER_MSR_SYSENTER_EIP, vcpu_mrs->gpregs_item.regs.eax);
	break;
    default:
	printf("unhandled write to MSR %x value %x:%x ip %x", 
	       msr, vcpu_mrs->gpregs_item.regs.edx, vcpu_mrs->gpregs_item.regs.eax, vcpu_mrs->gpregs_item.regs.eip );
	DEBUGGER_ENTER("UNIMPLEMENTED");
	break;
    }
    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
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
	    msr, value2, value1, vcpu_mrs->gpregs_item.regs.eip);

    vcpu_mrs->gpregs_item.regs.eax = value1;
    vcpu_mrs->gpregs_item.regs.eax = value2;
    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();

    return true;
}

bool handle_unknown_msr_write()
{
    L4_Word_t msr;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );
    
    printf("unhandled write to MSR %x value %x:%x ip %x", 
	   msr, vcpu_mrs->gpregs_item.regs.edx, vcpu_mrs->gpregs_item.regs.eax, vcpu_mrs->gpregs_item.regs.eip );

    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();
    return true;
}

bool handle_unknown_msr_read()
{
    L4_Word_t msr;
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    L4_StoreMR( 2, &msr );
    
    printf("read from unknown MSR %x ip %x", msr, vcpu_mrs->gpregs_item.regs.eip );
    DEBUGGER_ENTER("UNTESTED UNKNOWN MSR READ");


    switch (msr)
    {
    case 0x1a0:			// IA32_MISC_ENABLE
	vcpu_mrs->gpregs_item.regs.eax = 0xc00;
	vcpu_mrs->gpregs_item.regs.eax = 0;
    break;
    default:
	printf("unhandled read from MSR %x ip %x", 
	       msr, vcpu_mrs->gpregs_item.regs.eip );
	vcpu_mrs->gpregs_item.regs.eax = 0;
	vcpu_mrs->gpregs_item.regs.eax = 0;
    }

    vcpu_mrs->gpregs_item.regs.eip += vcpu_mrs->instr_len;
    vcpu_mrs->append_gpregs_item();

    return true;
}

#endif

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
    vcpu_mrs->append_seg_item(L4_CTRLXFER_CSREGS_ID, 0, 0, 0, 0, true);
    vcpu_mrs->append_seg_item(L4_CTRLXFER_SSREGS_ID, 0, 0, 0, 0);
    vcpu_mrs->load();
    DEBUGGER_ENTER("UNTESTED REQUEST STATE");
    // Read new items via exregs
    L4_ReadCtrlXferItems(get_vcpu().main_gtid);
    
    tag = L4_MsgTag();
    printf("unimplemented exregs store segreg item");
    UNIMPLEMENTED();
    //L4_StoreMRs(L4_UntypedWords(tag)+1, 5, (L4_Word_t*)&vcpu_mrs->seg_item[0]);
    //L4_StoreMRs(L4_UntypedWords(tag)+6, 5, (L4_Word_t*)&vcpu_mrs->seg_item[1]);
    //ASSERT(vcpu_mrs->seg_item[0].item.id == 4);
    //ASSERT(vcpu_mrs->seg_item[1].item.id == 5);

    if(!(vcpu_mrs->gpregs_item.regs.eflags & 0x200) && hw) {
	printf( "WARNING: hw interrupt injection with if flag disabled !!!\n");
	printf( "eflags is: %x\n", vcpu_mrs->gpregs_item.regs.eflags);
	goto erply;
    }

    //    printf("\nReceived int vector %x\n", vector);
    //    printf("Got: sp:%x, efl:%x, ip:%x, cs:%x, ss:%x\n", sp, eflags, ip, cs, ss);
    stack_addr = (vcpu_mrs->seg_item[1].regs.segreg << 4) + (vcpu_mrs->gpregs_item.regs.esp & 0xffff);
    
    UNIMPLEMENTED();
    //stack = (u16_t*) get_vcpu().get_map_addr( stack_addr );
    
    // push eflags, cs and ip onto the stack
    *(--stack) = vcpu_mrs->gpregs_item.regs.eflags & 0xffff;
    *(--stack) = vcpu_mrs->seg_item[0].regs.segreg & 0xffff;
    if (hw)
	*(--stack) = vcpu_mrs->gpregs_item.regs.eip & 0xffff;
    else
	*(--stack) = (vcpu_mrs->gpregs_item.regs.eip + vcpu_mrs->hvm.ilen) & 0xffff;

    if( vcpu_mrs->gpregs_item.regs.esp-6 < 0 )
	DEBUGGER_ENTER("stackpointer below segment");
    
    // get entry in interrupt vector table from guest
    UNIMPLEMENTED();
    //int_vector = (ia32_ive_t*) get_vcpu().get_map_addr( vector*4 );
    dprintf(debug_hvm_irq, "Ii: %x (%c), entry: %x, %x at: %x\n", 
	    vector, hw ? 'h' : 's',  int_vector->ip, int_vector->cs, 
	    (vcpu_mrs->seg_item[0].regs.base + vcpu_mrs->gpregs_item.regs.eip));
    

    vcpu_mrs->gpregs_item.regs.eip = int_vector->ip;
    vcpu_mrs->gpregs_item.regs.esp -= 6;
    vcpu_mrs->gpregs_item.regs.eflags &= 0x200;

    vcpu_mrs->append_gpregs_item();

    L4_Set(&vcpu_mrs->seg_item[0], L4_CTRLXFER_CSREGS_CS, int_vector->cs);
    L4_Set(&vcpu_mrs->seg_item[0], L4_CTRLXFER_CSREGS_CS_BASE, (int_vector->cs << 4 ));
    
 erply:

    return true;
}

#if 0

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

    linear_ip = (u8_t*) (vcpu_mrs->gpregs_item.regs.eip + vcpu_mrs->seg_item[0].regs.base);

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
					    operand_addr = vcpu_mrs->gpregs_item.regs.eax;
					    break;
					case 0x1:
					    operand_addr = vcpu_mrs->gpregs_item.regs.ecx;
					    break;
					case 0x2:
					    operand_addr = vcpu_mrs->gpregs_item.regs.edx;
					    break;
					case 0x3:
					    operand_addr = vcpu_mrs->gpregs_item.regs.ebx;
					    break;
					case 0x6:
					    operand_addr = vcpu_mrs->gpregs_item.regs.esi;
					    break;
					case 0x7:
					    operand_addr = vcpu_mrs->gpregs_item.regs.edi;
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
					    operand_addr = vcpu_mrs->gpregs_item.regs.esi;
					    break;
					case 0x5:
					    operand_addr = vcpu_mrs->gpregs_item.regs.edi;
					    break;
					case 0x7:
					    operand_addr = vcpu_mrs->gpregs_item.regs.ebx;
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
					operand_data = vcpu_mrs->gpregs_item.regs.eax;
					break;
				    case 0x1:
					operand_data = vcpu_mrs->gpregs_item.regs.ecx;
					break;
				    case 0x2:
					operand_data = vcpu_mrs->gpregs_item.regs.edx;
					break;
				    case 0x3:
					operand_data = vcpu_mrs->gpregs_item.regs.ebx;
					break;
				    case 0x4:
					operand_data = vcpu_mrs->gpregs_item.regs.esp;
					break;
				    case 0x5:
					operand_data = vcpu_mrs->gpregs_item.regs.ebp;
					break;
				    case 0x6:
					operand_data = vcpu_mrs->gpregs_item.regs.esi;
					break;
				    case 0x7:
					operand_data = vcpu_mrs->gpregs_item.regs.edi;
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
						      operand, vcpu_mrs->gpregs_item.regs.reg[(modrm & 0x7)]);
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
	    io.X.port		= vcpu_mrs->gpregs_item.regs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandMemory;


	    if (seg_ovr)
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (seg_ovr_base + (vcpu_mrs->gpregs_item.regs.esi & 0xffff))
		    : (seg_ovr_base + (vcpu_mrs->gpregs_item.regs.edi & 0xffff));
	    else
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (vcpu_mrs->seg_item[2].regs.base + (vcpu_mrs->gpregs_item.regs.esi & 0xffff))
		    : (vcpu_mrs->seg_item[3].regs.base + (vcpu_mrs->gpregs_item.regs.edi & 0xffff));

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
		return handle_io_write(io, operand, vcpu_mrs->gpregs_item.regs.eax);
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
	    io.X.port		= vcpu_mrs->gpregs_item.regs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandRegister;
	    DEBUGGER_ENTER("UNTESTED EAX REF 3");
	    operand.reg.index	= L4_CTRLXFER_GPREGS_EAX;

	    if(*linear_ip >= 0xee) // write
		return handle_io_write(io, operand, vcpu_mrs->gpregs_item.regs.eax);
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
	    L4_Set(&vcpu_mrs->cr_item[2], L4_CTRLXFER_CREGS_CR2, addr);
	}
    }

    dprintf(debug_hvm_fault,  "exception %x %x %x ip %x\n", except.raw, err_code, addr, vcpu_mrs->gpregs_item.regs.eip); 
    DEBUGGER_ENTER("UNTESTED INJECT IRQ");

    L4_ExcInjectCtrlXferItemSet(&vcpu_mrs->exc_item, except.raw, 0, 0);

    return true;
}


#endif


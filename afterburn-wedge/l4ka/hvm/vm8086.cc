/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     vm8086.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
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
#include INC_ARCH(hvm_vmx.h)

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

    if(!(vcpu_mrs->gpr_item.regs.eflags & 0x200) && hw) {
	printf( "WARNING: hw interrupt injection with if flag disabled !!!\n");
	printf( "eflags is: %x\n", vcpu_mrs->gpr_item.regs.eflags);
	goto erply;
    }

    //    printf("\nReceived int vector %x\n", vector);
    //    printf("Got: sp:%x, efl:%x, ip:%x, cs:%x, ss:%x\n", sp, eflags, ip, cs, ss);
    stack_addr = (vcpu_mrs->seg_item[1].regs.segreg << 4) + (vcpu_mrs->gpr_item.regs.esp & 0xffff);
    
    UNIMPLEMENTED();
    //stack = (u16_t*) get_vcpu().get_map_addr( stack_addr );
    
    // push eflags, cs and ip onto the stack
    *(--stack) = vcpu_mrs->gpr_item.regs.eflags & 0xffff;
    *(--stack) = vcpu_mrs->seg_item[0].regs.segreg & 0xffff;
    if (hw)
	*(--stack) = vcpu_mrs->gpr_item.regs.eip & 0xffff;
    else
	*(--stack) = (vcpu_mrs->gpr_item.regs.eip + vcpu_mrs->hvm.ilen) & 0xffff;

    if( vcpu_mrs->gpr_item.regs.esp-6 < 0 )
	DEBUGGER_ENTER("stackpointer below segment");
    
    // get entry in interrupt vector table from guest
    UNIMPLEMENTED();
    //int_vector = (ia32_ive_t*) get_vcpu().get_map_addr( vector*4 );
    dprintf(debug_hvm_irq, "Ii: %x (%c), entry: %x, %x at: %x\n", 
	    vector, hw ? 'h' : 's',  int_vector->ip, int_vector->cs, 
	    (vcpu_mrs->seg_item[0].regs.base + vcpu_mrs->gpr_item.regs.eip));
    

    vcpu_mrs->gpr_item.regs.eip = int_vector->ip;
    vcpu_mrs->gpr_item.regs.esp -= 6;
    vcpu_mrs->gpr_item.regs.eflags &= 0x200;

    vcpu_mrs->append_gpr_item();

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

    linear_ip = (u8_t*) (vcpu_mrs->gpr_item.regs.eip + vcpu_mrs->seg_item[0].regs.base);

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
					    operand_addr = vcpu_mrs->gpr_item.regs.eax;
					    break;
					case 0x1:
					    operand_addr = vcpu_mrs->gpr_item.regs.ecx;
					    break;
					case 0x2:
					    operand_addr = vcpu_mrs->gpr_item.regs.edx;
					    break;
					case 0x3:
					    operand_addr = vcpu_mrs->gpr_item.regs.ebx;
					    break;
					case 0x6:
					    operand_addr = vcpu_mrs->gpr_item.regs.esi;
					    break;
					case 0x7:
					    operand_addr = vcpu_mrs->gpr_item.regs.edi;
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
					    operand_addr = vcpu_mrs->gpr_item.regs.esi;
					    break;
					case 0x5:
					    operand_addr = vcpu_mrs->gpr_item.regs.edi;
					    break;
					case 0x7:
					    operand_addr = vcpu_mrs->gpr_item.regs.ebx;
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
					operand_data = vcpu_mrs->gpr_item.regs.eax;
					break;
				    case 0x1:
					operand_data = vcpu_mrs->gpr_item.regs.ecx;
					break;
				    case 0x2:
					operand_data = vcpu_mrs->gpr_item.regs.edx;
					break;
				    case 0x3:
					operand_data = vcpu_mrs->gpr_item.regs.ebx;
					break;
				    case 0x4:
					operand_data = vcpu_mrs->gpr_item.regs.esp;
					break;
				    case 0x5:
					operand_data = vcpu_mrs->gpr_item.regs.ebp;
					break;
				    case 0x6:
					operand_data = vcpu_mrs->gpr_item.regs.esi;
					break;
				    case 0x7:
					operand_data = vcpu_mrs->gpr_item.regs.edi;
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
						      operand, vcpu_mrs->gpr_item.regs.reg[(modrm & 0x7)]);
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
	    io.X.port		= vcpu_mrs->gpr_item.regs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandMemory;


	    if (seg_ovr)
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (seg_ovr_base + (vcpu_mrs->gpr_item.regs.esi & 0xffff))
		    : (seg_ovr_base + (vcpu_mrs->gpr_item.regs.edi & 0xffff));
	    else
		mem_addr = (*linear_ip >= 0x6e) 
		    ? (vcpu_mrs->seg_item[2].regs.base + (vcpu_mrs->gpr_item.regs.esi & 0xffff))
		    : (vcpu_mrs->seg_item[3].regs.base + (vcpu_mrs->gpr_item.regs.edi & 0xffff));

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
		return handle_io_write(io, operand, vcpu_mrs->gpr_item.regs.eax);
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
	    io.X.port		= vcpu_mrs->gpr_item.regs.edx & 0xffff;
	    io.X.access_size	= data_size;

	    operand.raw		= 0;
	    operand.X.type	= L4_OperandRegister;
	    DEBUGGER_ENTER("UNTESTED EAX REF 3");
	    operand.reg.index	= L4_CTRLXFER_GPREGS_EAX;

	    if(*linear_ip >= 0xee) // write
		return handle_io_write(io, operand, vcpu_mrs->gpr_item.regs.eax);
	    else
		return handle_io_read(io, operand, 0);

	case 0xf4:				// hlt
	    return handle_instruction(L4_VcpuIns_hlt);
	}

    return false;
}


#endif

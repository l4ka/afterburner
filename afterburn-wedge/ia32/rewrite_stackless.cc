/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe and
 *                      National ICT Australia (NICTA)
 *
 * File path:     afterburn-wedge/ia32/rewrite_stackless.cc
 * Description:   The logic for rewriting an afterburned guest OS.
 *                The rewritten code assumes that it can use the 
 *                guest OS stack.
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

#include INC_ARCH(rewrite_stackless.h)

#include <bind.h>
#include <string.h>
#include <templates.h>


#if defined(CONFIG_IA32_HARDWARE_SEGMENTS)
static const bool hardware_segments = true;
#else
static const bool hardware_segments = false;
#endif


static word_t cli_remain = ~0;
static word_t sti_remain = ~0;
static word_t popf_remain = ~0;
static word_t pushf_remain = ~0;
static word_t iret_remain = ~0;
static word_t pop_ds_remain = ~0;
static word_t pop_es_remain = ~0;
static word_t hlt_remain = ~0;
static word_t push_ds_remain = ~0;
static word_t push_es_remain = ~0;
static word_t push_fs_remain = ~0;
static word_t push_gs_remain = ~0;
static word_t read_seg_remain = ~0;
static word_t write_seg_remain = ~0;
static word_t mov_segofs_remain = ~0;




burn_func_t burn_write_cr[] = {
    burn_write_cr0, 0, burn_write_cr2, burn_write_cr3, burn_write_cr4,
};

//burn_func_t burn_read_cr[] = {
//    burn_read_cr0, 0, burn_read_cr2, burn_read_cr3, burn_read_cr4,
//};

static u8_t *
apply_port_in_patchup( u8_t *newops, u8_t port, bool &handled )
{
    void *which = NULL;
    stack_offset = 0;
    handled = true;
    switch( port ) {
	case 0x20: which = (void *)device_8259_0x20_in; break;
	case 0x21: which = (void *)device_8259_0x21_in; break;
	case 0xa0: which = (void *)device_8259_0xa0_in; break;
	case 0xa1: which = (void *)device_8259_0xa1_in; break;
	default:
		   handled = false;
		   return newops;
    }

    newops = push_reg( newops, OP_REG_ECX );
    newops = push_reg( newops, OP_REG_EDX );
    newops = op_call( newops, which );
    newops = pop_reg( newops, OP_REG_EDX );
    newops = pop_reg( newops, OP_REG_ECX );

    return newops;
}

static u8_t *
apply_port_out_patchup( u8_t *newops, u8_t port, bool &handled )
{
    void *which = NULL;
    stack_offset = 0;
    handled = true;
    switch( port ) {
	case 0x20: which = (void *)device_8259_0x20_out; break;
	case 0x21: which = (void *)device_8259_0x21_out; break;
	case 0xa0: which = (void *)device_8259_0xa0_out; break;
	case 0xa1: which = (void *)device_8259_0xa1_out; break;
	default:
		   handled = false;
		   return newops;
    }

    newops = push_reg( newops, OP_REG_ECX );
    newops = push_reg( newops, OP_REG_EDX );
    newops = op_call( newops, which );
    newops = pop_reg( newops, OP_REG_EDX );
    newops = pop_reg( newops, OP_REG_ECX );

    return newops;
}

static void
init_patchup()
{
#if  defined(CONFIG_WEDGE_L4KA) || defined(CONFIG_WEDGE_XEN) || defined(CONFIG_WEDGE_KAXEN)
	curr_virt_addr = 0;
#endif
#if defined(CONFIG_WEDGE_LINUX)
	curr_virt_addr = -1 * get_vcpu().get_kernel_vaddr();
#endif
}

static bool
apply_patchup( u8_t *opstream, u8_t *opstream_end )
{
	bool handled;
	u32_t value;
	u32_t jmp_target;
	u16_t segment;
	ia32_modrm_t modrm;
	u8_t suffixes[6];

	stack_offset = 0;

	u8_t *newops = opstream;
	bool rep_prefix = false;
	bool repne_prefix = false;
	bool operand_size_prefix = false;
	bool address_size_prefix = false;
	bool segment_prefix = false;
	prefix_e prefix;
	
	while( 1 ) {

	/* Instruction decoding */
	switch(opstream[0]) {
	    case prefix_lock:
		*newops++ = OP_NOP1;
		opstream++;
		break;
	    case prefix_repne:
		printf( "repne prefix not supported @ %x\n", opstream);
		return false;
		repne_prefix = true;
		opstream++;
		continue;
	    case prefix_rep:
		printf( "rep prefix not supported @ %x\n", opstream);
		return false;
		rep_prefix = true;
		opstream++;
		continue;
	    case prefix_operand_size:
		operand_size_prefix = true;
		opstream++;
		continue;
	    case prefix_address_size:
		address_size_prefix = true;
		opstream++;
		continue;
	    case prefix_cs:
	    case prefix_ss:
	    case prefix_ds:
	    case prefix_es:
		printf( "Ignored CS/SS/DS/ES Segment prefix @ %x\n", opstream);
		opstream++;
		continue;
	    case prefix_fs:
	    case prefix_gs:
		segment_prefix = true;
		opstream++;
		continue;
	    case OP_HLT:
		newops = op_call(newops, (void*) burn_interruptible_hlt);
		update_remain( hlt_remain, newops, opstream_end );
		break;
	    case OP_INT3:
#if defined(CONFIG_WEDGE_L4KA)
		// Permit int3, for the L4Ka::Pistachio kernel debugger.
		// But the kernel debugger uses a protocol based on the
		// instructions following the int3.  So move the int3 to
		// the end of the NOPs.
		opstream_end[-1] = opstream[0];
		opstream[0] = OP_NOP1;
		break;
#else
		opstream[1] = 3; // Cheat, and reuse the fall through code.
		// Fall through.
#endif
	    case OP_INT:
		value = opstream[1];
		newops = push_word( newops, value ); // Vector no.
		newops = op_call( newops, (void*) burn_int );
		// Note: the stack must be cleaned by burn_int.
		// 10 bytes of new instructions, so jump back 10+2 using a 
		// 2-byte jump.
		*newops++ = 0xeb;	// JMP rel8
		*newops++ = (u8_t)-12;	// JMP -12
		break;
	    case OP_IRET:
		newops = op_jmp(newops, (void*) burn_iret);
		update_remain( iret_remain, newops, opstream_end );
		break;
	    case OP_LRET:
		newops = op_jmp(newops, (void*) burn_lret);
		break;
    	    case OP_JMP_FAR:
		/* We make the hellish assumption that now we are no 
		 * longer in physical mode!  We assume that the last
		 * afterburned instruction prior to this enabled
		 * paging in cr0.  Since paging is already enabled,
		 * we need to use proper relative offsets, even for this instr.
		 */ 
#if defined(CONFIG_WEDGE_L4KA) || defined(CONFIG_WEDGE_KAXEN)
		curr_virt_addr = get_vcpu().get_kernel_vaddr();
#endif
		jmp_target = *((u32_t*)&opstream[1]);
		segment = *((u16_t*)&opstream[5]);
		newops = push_word( newops, segment );
		newops = push_word( newops, jmp_target ); // Return addr.
		newops = op_jmp( newops, (void *)burn_write_cs );
#if defined(CONFIG_WEDGE_XEN)
		curr_virt_addr = get_vcpu().get_kernel_vaddr();
#endif
#if defined(CONFIG_WEDGE_LINUX)
		curr_virt_addr = 0;
#endif
		break;
	    case OP_OUT:
		value = opstream[1]; // Port number, 8-bit immediate
		newops = apply_port_out_patchup( newops, value, handled );
		if( !handled ) {
		    if( operand_size_prefix )
			newops = push_byte(newops, 16);
		    else
			newops = push_byte(newops, 32);
		    newops = push_word(newops, value);
		    newops = op_call(newops, (void*) burn_out);
		    newops = clean_stack( newops, 8 );
		}
		break;
	    case OP_OUT_DX:
		if( operand_size_prefix )
		    newops = push_byte(newops, 16);
		else
		    newops = push_byte(newops, 32);
		newops = push_reg(newops, OP_REG_EDX);
		newops = op_call(newops, (void*) burn_out);
		newops = clean_stack( newops, 8 );
		break;
	    case OP_OUTB:
		value = opstream[1];
		newops = apply_port_out_patchup( newops, value, handled );
		if( !handled ) {
		    newops = push_byte(newops, 0x8);
		    newops = push_word(newops, value);
		    newops = op_call(newops, (void*) burn_out);
		    newops = clean_stack( newops, 8 );
		}
		break;
	    case OP_OUTB_DX:
		newops = push_byte(newops, 0x8);
		newops = push_reg(newops, OP_REG_EDX);
		newops = op_call(newops, (void*) burn_out);
		newops = clean_stack( newops, 8 );
		break;
	    case OP_IN:
		value = opstream[1];
		newops = apply_port_in_patchup( newops, value, handled );
		if( !handled ) {
		    if( operand_size_prefix )
	    		newops = push_byte(newops, 16);
    		    else
			newops = push_byte(newops, 32);
		    newops = push_word(newops, value);
		    newops = op_call(newops, (void*) burn_in);
		    newops = clean_stack( newops, 8 );
		}
		break;
	    case OP_IN_DX:
		if( operand_size_prefix )
		    newops = push_byte(newops, 16);
		else
		    newops = push_byte(newops, 32);
		newops = push_reg(newops, OP_REG_EDX);
		newops = op_call(newops, (void*) burn_in);
		newops = clean_stack( newops, 8 );
		break;
	    case OP_INB:
		value = opstream[1];
		newops = apply_port_in_patchup( newops, value, handled );
		if( !handled ) {
		    newops = push_byte(newops, 0x8);
		    newops = push_word(newops, value);
		    newops = op_call(newops, (void*) burn_in);
		    newops = clean_stack( newops, 8 );
		}
		break;
	    case OP_INB_DX:
		newops = push_byte(newops, 0x8);
		newops = push_reg(newops, OP_REG_EDX);
		newops = op_call(newops, (void*) burn_in);
		newops = clean_stack( newops, 8 );
		break;

	    case OP_PUSHF:
		newops = newops_pushf(newops);
		update_remain( pushf_remain, newops, opstream_end );
		break;
	    case OP_POPF:
#if defined(CONFIG_IA32_STRICT_IRQ) 
#if !defined(CONFIG_DEVICE_APIC)
		newops = pop_mem32( newops, (word_t)&get_cpu().flags.x.raw );
		newops = cmp_mem_imm8( newops, (word_t)&get_intlogic().vector_cluster, 0 );
		newops = jmp_short_zero( newops, word_t(opstream_end) );
		newops = op_call(newops, (void*) burn_deliver_interrupt);
		newops = pop_mem32( newops, (word_t)&get_cpu().flags.x.raw );
#else
		newops = op_call( newops, (void *)burn_popf );
		newops = op_popf( newops ); // Pop the unprivileged flags.
#endif
#else  /* defined(CONFIG_IA32_STRICT_IRQ) */
		newops = pop_mem32( newops, (word_t)&get_cpu().flags.x.raw );
		update_remain( popf_remain, newops, opstream_end );
#endif
		
		update_remain( popf_remain, newops, opstream_end );
		break;
		
	    case OP_MOV_TOMEM32:
		ASSERT (segment_prefix);
		opstream--;
		prefix = (prefix_e) opstream[0];	
		modrm.x.raw = opstream[2];
	
		memcpy( suffixes, &opstream[3], sizeof(suffixes) );
		newops = push_reg( newops, modrm.get_reg() );  // Save
		newops = push_reg( newops, modrm.get_reg() );  // Arg 1
		newops = lea_modrm( newops, modrm.get_reg(), modrm, suffixes );
		newops = push_reg( newops, modrm.get_reg() ); // Arg2
		
		switch (prefix)
		{
		    case prefix_fs:
			//printf( "mov reg -> fs:addr @ " << (void *) opstream << "\n");
			newops = op_call( newops, (void *)burn_mov_tofsofs);
			break;
		    case prefix_gs:
			printf( "mov reg -> gs:addr @ %x\n", opstream);
			DEBUGGER_ENTER_M("BUG");
			break;
		    default:
			printf( "unsupported prefix %x mov reg -> seg:ofs @ %x\n", prefix, opstream); 
			DEBUGGER_ENTER_M("BUG");
			break;
		}	
		
		newops = clean_stack( newops, 8 );
		newops = pop_reg( newops, modrm.get_reg() );  // Restore
		update_remain( mov_segofs_remain, newops, opstream_end );
		break;
		
	    case OP_MOV_FROMMEM32:
		ASSERT (segment_prefix);
		opstream--;
		prefix = (prefix_e) opstream[0];	
		modrm.x.raw = opstream[2];
 	
		memcpy( suffixes, &opstream[3], sizeof(suffixes) );
		newops = lea_modrm( newops, modrm.get_reg(), modrm, suffixes );
		newops = push_reg( newops, modrm.get_reg() ); // Arg1
		
		switch (prefix)
		{
		    case prefix_fs:
 			//printf( "mov fs:addr -> reg @ " << (void *) opstream << "\n");
			newops = op_call( newops, (void *)burn_mov_fromfsofs);
			break;
		    case prefix_gs:	
			printf( "mov gs:addr -> reg @ %x\n", prefix, opstream); 
			DEBUGGER_ENTER_M("BUG");
			break;
		    default:
			printf( "unsupported prefix %x mov seg:ofs -> reg @ %x\n", prefix, opstream); 
			break;
			DEBUGGER_ENTER_M("BUG");
		}	
		
		newops = pop_reg( newops, modrm.get_reg() );
		update_remain( mov_segofs_remain, newops, opstream_end );
		break;
		
	    case OP_MOV_TOSEGOFS:
		ASSERT (segment_prefix);
		opstream--;
		prefix = (prefix_e) opstream[0];	
		value = * (u32_t *) &opstream[2];
		
		memcpy( suffixes, &opstream[3], sizeof(suffixes) );
		newops = push_reg( newops, OP_REG_EAX );  // Save
		newops = push_reg( newops, OP_REG_EAX );  // Arg1
		newops = push_word(newops, value);	  // Arg2
		
		switch (prefix)
		{
		    case prefix_fs:
			//printf( "mov eax -> fs:addr @ " << (void *) opstream << "\n");
			newops = op_call( newops, (void *)burn_mov_tofsofs );
			break;
		    case prefix_gs:
			printf( "mov eax -> gs:addr @ %x\n", prefix, opstream); 
			DEBUGGER_ENTER_M("BUG");
			break;
		    default:
			printf( "unsupported prefix %x mov eax -> seg:ofs @ %x\n", prefix, opstream); 
			DEBUGGER_ENTER_M("BUG");
			break;
		}	
 		newops = clean_stack( newops, 8 );
		newops = pop_reg( newops, modrm.get_reg() );  // Restore
		update_remain( mov_segofs_remain, newops, opstream_end );
		break;
	    case OP_MOV_FROMSEGOFS:
		ASSERT (segment_prefix);
		opstream--;
		prefix = (prefix_e) opstream[0];	
		value = * (u32_t *) &opstream[2];
		
		memcpy( suffixes, &opstream[3], sizeof(suffixes) );
		newops = push_word(newops, value);
		
		switch (prefix)
		{
		    case prefix_fs:
			//printf( "mov fs:addr -> eax @ " << (void *) opstream << "\n");
			newops = op_call( newops, (void *)burn_mov_fromfsofs );
			break;
		    case prefix_gs:
			printf( "mov gs:addr -> eax @ %x\n", prefix, opstream); 
			DEBUGGER_ENTER_M("BUG");
			break;
		    default:
			printf( "unsupported prefix %x mov seg:ofs -> eax @ %x\n", prefix, opstream); 
			break;
		}	
		newops = pop_reg( newops, OP_REG_EAX );
		update_remain( mov_segofs_remain, newops, opstream_end );
		break;
	    case OP_MOV_TOSEG:
		if( hardware_segments )
		    break;
		modrm.x.raw = opstream[1];
		memcpy( suffixes, &opstream[2], sizeof(suffixes) );
		newops = newops_mov_toseg(opstream, modrm, suffixes);
		update_remain( write_seg_remain, newops, opstream_end );
		break;
	    case OP_PUSH_CS:
		if( hardware_segments )
		    break;
		newops = newops_pushcs(newops);
		break;
	    case OP_PUSH_SS:
		if( hardware_segments )
		    break;
		newops = newops_pushss(newops);
		break;
	    case OP_POP_SS:
		if( hardware_segments )
		    break;
		newops = newops_popss(newops);
		break;
	    case OP_PUSH_DS:
		if( hardware_segments )
		    break;
		newops = newops_pushds(newops);
		update_remain( push_ds_remain, newops, opstream_end );
		break;
	    case OP_POP_DS:
		if( hardware_segments )
		    break;
		newops = newops_popds(newops);
		update_remain( pop_ds_remain, newops, opstream_end );
		break;
	    case OP_PUSH_ES:
		if( hardware_segments )
		    break;
		newops = newops_pushes(newops);
		update_remain( push_es_remain, newops, opstream_end );
		break;
	    case OP_POP_ES:
		if( hardware_segments )
		    break;
		newops = newops_popes(newops);
		update_remain( pop_es_remain, newops, opstream_end );
		break;
	    case OP_MOV_FROMSEG:
		if( hardware_segments )
		    break;
		modrm.x.raw = opstream[1];
		memcpy( suffixes, &opstream[2], sizeof(suffixes) );
		newops = newops_movfromseg(newops, modrm, suffixes);
		update_remain( read_seg_remain, newops, opstream_end );
		break;
	    case OP_CLI:
		newops = newops_cli(newops);
		update_remain( cli_remain, newops, opstream_end );
		break;
	    case OP_STI:
#if defined(CONFIG_IA32_STRICT_IRQ) 
#if !defined(CONFIG_DEVICE_APIC)
		newops = bts_mem32_immediate( newops, (word_t)&get_cpu().flags.x.raw, 9 );
		newops = cmp_mem_imm8( newops, (word_t)&get_intlogic().vector_cluster, 0 );
		newops = jmp_short_zero( newops, word_t(opstream_end) );
		newops = op_call(newops, (void*) burn_deliver_interrupt);
#else
		newops = op_call(newops, (void*) burn_sti);
#endif
#else  /* defined(CONFIG_IA32_STRICT_IRQ) */
		newops = bts_mem32_immediate(newops, (word_t)&get_cpu().flags.x.raw, 9);
#endif /* defined(CONFIG_IA32_STRICT_IRQ) */
		update_remain( sti_remain, newops, opstream_end );
		break;
	    case OP_2BYTE:
		switch(opstream[1]) {
		    case OP_WBINVD:
			newops = op_call( newops, (void *)burn_wbinvd );
			break;
		    case OP_INVD:
			newops = op_call( newops, (void *)burn_invd );
			break;
		    case OP_UD2:
			newops = op_call( newops, (void *)burn_ud2 );
			break;
		    case OP_PUSH_FS:
			if( hardware_segments )
			    break;
			newops = newops_pushfs(newops);
			update_remain( push_fs_remain, newops, opstream_end );
			break;
		    case OP_POP_FS:
			if( hardware_segments )
			    break;
			newops = newops_popfs(newops);
			break;
		    case OP_PUSH_GS:
			if( hardware_segments )
			    break;
			newops = newops_pushgs(newops);
			update_remain( push_gs_remain, newops, opstream_end );
			break;
		    case OP_POP_GS:
			if( hardware_segments )
			    break;
			newops = newops_popgs(newops);
			break;
		    case OP_CPUID:
			newops = op_call( newops, (void *)burn_cpuid );
			break;
	    	    case OP_LSS:
			// TODO: delay preemption during this operation.
			modrm.x.raw = opstream[2];
			memcpy( suffixes, &opstream[3], sizeof(suffixes) );
			newops = push_reg( newops, OP_REG_EAX ); // Preserve
			newops = lea_modrm( newops, OP_REG_EAX, modrm, 
				suffixes );
			newops = op_call( newops, (void *)burn_lss );
			newops = pop_reg( newops, OP_REG_EAX ); // Restore
			newops = mov_from_modrm( newops, OP_REG_ESP, modrm,
				suffixes );
			break;
		    case OP_MOV_FROMCREG:
			// The modrm reg field is the control register.
			// The modrm r/m field is the general purpose register.
			modrm.x.raw = opstream[2];
			newops = newops_movfromcreg(newops, modrm, suffixes);
			break;
		    case OP_MOV_FROMDREG:
			// The modrm reg field is the debug register.
			// The modrm r/m field is the general purpose register.
			modrm.x.raw = opstream[2];
			ASSERT( modrm.is_register_mode() );
			newops = push_reg( newops, OP_REG_EAX ); // Allocate
			newops = push_byte( newops, modrm.get_reg() );
			newops = op_call( newops, (void *)burn_read_dr );
			newops = clean_stack( newops, 4 );
			newops = pop_reg( newops, modrm.get_rm() );
			break;
		    case OP_MOV_TOCREG:
			// The modrm reg field is the control register.
			// The modrm r/m field is the general purpose register.
			modrm.x.raw = opstream[2];
			ASSERT( modrm.is_register_mode() );
			ASSERT( (modrm.get_reg() != 1) && 
				(modrm.get_reg() <= 4) );
			newops = push_reg( newops, modrm.get_rm() );
			newops = op_call( newops, (void *)burn_write_cr[modrm.get_reg()] );
			newops = clean_stack( newops, 4 );
			break;
		    case OP_MOV_TODREG:
			// The modrm reg field is the debug register.
			// The modrm r/m field is the general purpose register.
			modrm.x.raw = opstream[2];
			ASSERT( modrm.is_register_mode() );
			newops = push_reg( newops, modrm.get_rm() );
			newops = push_byte( newops, modrm.get_reg() );
			newops = op_call( newops, (void *)burn_write_dr );
			newops = clean_stack( newops, 8 );
			break;
		    case OP_CLTS:
			newops = op_call(newops, (void*) burn_clts);
			break;
		    case OP_LLTL:
			modrm.x.raw = opstream[2]; 
			memcpy( suffixes, &opstream[3], sizeof(suffixes) );
			newops = push_reg( newops, OP_REG_EAX );
			newops = movzx_modrm( newops, 
				OP_REG_EAX, modrm, suffixes );
			if( modrm.get_opcode() == 3 )
			    newops = op_call(newops, (void*) burn_ltr);
			else if( modrm.get_opcode() == 2 )
			    newops = op_call(newops, (void*) burn_lldt);
			else if( modrm.get_opcode() == 1 )
			    newops = op_call(newops, (void*) burn_str);
			else {
			    printf( "Unknown lltl sub type @ %x\n", opstream);
			    return false;
			}
			newops = pop_reg( newops, OP_REG_EAX );
			break;
		    case OP_LDTL:
			modrm.x.raw = opstream[2]; 
			memcpy( suffixes, &opstream[3], sizeof(suffixes) );
			newops = push_reg( newops, OP_REG_EAX );  // Preserve
			newops = lea_modrm( newops, OP_REG_EAX, modrm, suffixes );
			switch (modrm.get_opcode())
			{
			    case 0: // sgdt
				newops = op_call(newops, (void*)burn_unimplemented);
				break;
			    case 1: // sidt
				newops = op_call(newops, (void*)burn_unimplemented);
				break;
			    case 2: // lgdt
				newops = op_call( newops, (void *)burn_lgdt );
				break;
			    case 3: // lidt
				newops = op_call( newops, (void *)burn_lidt );
				break;
			    case 4: // smsw
				newops = op_call(newops, (void*)burn_unimplemented);
				break;
			    case 6: // lmsw
				newops = op_call(newops, (void*)burn_unimplemented);
				break;
			    case 7: // invlpg
				newops = op_call( newops, (void*)burn_invlpg);
				break;
			    default :
				printf( "Unknown opcode extension for ldtl @ %x\n", opstream);
				printf( "modrm.get_opcode() %x\n", modrm.get_opcode());
				return false;
				break;
			}
			newops = pop_reg( newops, OP_REG_EAX );  // Restore
			break;
		    case OP_WRMSR:
			newops = op_call(newops, (void*) burn_wrmsr);
			break;
		    case OP_RDMSR:
			newops = op_call(newops, (void*) burn_rdmsr);
			break;
		    default:
			printf( "Unknown 2 byte opcode @ %x\n", opstream);
			return false;
		}
		break;
	    default:
		printf( "Unknown opcode %x @ %x\n", (u32_t) opstream[0] , opstream);
		return false;
	}

	break;
	}

	if( newops > opstream_end ) {
	    printf( "Insufficient space for the afterburner instructions, @ %x, need %d bytes\n",
		    opstream_end, (newops - opstream_end));
	    return false;
	}

	return true;
}

bool
vmi_apply_patchups( vmi_annotation_t *annotations, word_t total, word_t vaddr_offset )
{
    init_patchup();
    for( word_t i = 0; i < total; i++ )
    {
	u8_t *ops_start = (u8_t *)(annotations[i].padded_begin - vaddr_offset);
	u8_t *ops_end = ops_start + annotations[i].padded_len;
	bool good = apply_patchup( ops_start, ops_end );
	if( !good )
	    return false;
    }

    dprintf(debug_patchup, "Total VMI patchups: %d\n", total);
    return true; 
}

bool
arch_apply_patchups( patchup_info_t *patchups, word_t total, word_t vaddr_offset )
{
    init_patchup();
    word_t back_to_back = 0;
    
    dprintf(debug_patchup, "ARCH patchups @ %x count %d\n", patchups, total);
    for( word_t i = 0; i < total; i++ )
    {
	bool good = apply_patchup( (u8_t *)(patchups[i].start - vaddr_offset),
				   (u8_t *)(patchups[i].end - vaddr_offset));
	if( !good )
	    return false;

	if( i && (patchups[i].start == patchups[i-1].end) ) {
//	    printf( "contiguous: " << (void *)patchups[i-1].start << ' ' 
//		<< (void *)patchups[i].start << '\n';
	    back_to_back++;
	}
    }

    dprintf(debug_patchup,  "contiguous patch-ups: %d", back_to_back );
    dprintf(debug_patchup,  "cli space %d", cli_remain );
    dprintf(debug_patchup,  "sti space %d", sti_remain );
    dprintf(debug_patchup,  "popf space %d", popf_remain );
    dprintf(debug_patchup,  "pushf space %d", pushf_remain );
    dprintf(debug_patchup,  "iret space %d", iret_remain );
    dprintf(debug_patchup,  "write ds space %d", pop_ds_remain );
    dprintf(debug_patchup,  "write es space %d", pop_es_remain );
    dprintf(debug_patchup,  "hlt space %d", hlt_remain );
    dprintf(debug_patchup,  "read ds space %d", push_ds_remain );
    dprintf(debug_patchup,  "read es space %d", push_es_remain );
    dprintf(debug_patchup,  "read fs space %d", push_fs_remain );
    dprintf(debug_patchup,  "read gs space %d", push_gs_remain );
    dprintf(debug_patchup,  "read seg space %d", read_seg_remain );
    dprintf(debug_patchup,  "write seg space %d", write_seg_remain );

    dprintf(debug_patchup, "Total patchups: %d\n", total);
    return true;
}

static bool
apply_device_patchup( u8_t *opstream, u8_t *opstream_end, 
	word_t read_func, word_t write_func, word_t xchg_func,
	word_t or_func, word_t and_func )
{
    u8_t *newops = opstream;
    ia32_modrm_t modrm;
    u8_t suffixes[6];

    stack_offset = 0;

    switch( opstream[0] )
    {
	case 0x09: // OR r32, r/m32
	    modrm.x.raw = opstream[1];
	    memcpy( suffixes, &opstream[2], sizeof(suffixes) );
    	    // Preserve according to the C calling conventions.
	    newops = push_reg( newops, OP_REG_EAX );
	    newops = push_reg( newops, OP_REG_ECX );
	    newops = push_reg( newops, OP_REG_EDX );
	    // We want the source in eax, and the target in edx.  The
	    // target is the device memory address.
	    if( modrm.get_reg() == OP_REG_EAX )
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    else if( modrm.get_reg() != OP_REG_EDX ) {
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, modrm.get_reg(), OP_REG_EAX );
	    }
	    else if( modrm.get_rm() == 0 ) {
	     	// The source is in EDX, and the target is in EAX.  Store the 
		// src temporarily in ECX.
    		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_ECX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, OP_REG_ECX, OP_REG_EAX );
	    }
	    else {
		// The source is in EDX, but the target is not in EAX.
		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_EAX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    }
	    // Call the write function.
	    ASSERT( or_func );
	    newops = op_call( newops, (void *)or_func );
	    // Restore the clobbered registers.
	    newops = pop_reg( newops, OP_REG_EDX );
	    newops = pop_reg( newops, OP_REG_ECX );
	    newops = pop_reg( newops, OP_REG_EAX );
	    break;
	case 0x21: // AND r32, r/m32
	    modrm.x.raw = opstream[1];
	    memcpy( suffixes, &opstream[2], sizeof(suffixes) );
    	    // Preserve according to the C calling conventions.
	    newops = push_reg( newops, OP_REG_EAX );
	    newops = push_reg( newops, OP_REG_ECX );
	    newops = push_reg( newops, OP_REG_EDX );
	    // We want the source in eax, and the target in edx.  The
	    // target is the device memory address.
	    if( modrm.get_reg() == OP_REG_EAX )
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    else if( modrm.get_reg() != OP_REG_EDX ) {
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, modrm.get_reg(), OP_REG_EAX );
	    }
	    else if( modrm.get_rm() == 0 ) {
	     	// The source is in EDX, and the target is in EAX.  Store the 
		// src temporarily in ECX.
    		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_ECX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, OP_REG_ECX, OP_REG_EAX );
	    }
	    else {
		// The source is in EDX, but the target is not in EAX.
		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_EAX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    }
	    // Call the write function.
	    ASSERT( and_func );
	    newops = op_call( newops, (void *)and_func );
	    // Restore the clobbered registers.
	    newops = pop_reg( newops, OP_REG_EDX );
	    newops = pop_reg( newops, OP_REG_ECX );
	    newops = pop_reg( newops, OP_REG_EAX );
	    break;
	case 0x89: // Move r32 to r/m32
	    modrm.x.raw = opstream[1];
	    memcpy( suffixes, &opstream[2], sizeof(suffixes) );

	    // Preserve according to the C calling conventions.
	    newops = push_reg( newops, OP_REG_EAX );
	    newops = push_reg( newops, OP_REG_ECX );
	    newops = push_reg( newops, OP_REG_EDX );
	    // We want the source in eax, and the target in edx.  The
	    // target is the device memory address.
	    if( modrm.get_reg() == OP_REG_EAX )
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    else if( modrm.get_reg() != OP_REG_EDX ) {
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, modrm.get_reg(), OP_REG_EAX );
	    }
	    else if( modrm.get_rm() == 0 ) {
	     	// The source is in EDX, and the target is in EAX.  Store the 
		// src temporarily in ECX.
    		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_ECX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, OP_REG_ECX, OP_REG_EAX );
	    }
	    else {
		// The source is in EDX, but the target is not in EAX.
		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_EAX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    }
	    // Call the write function.
	    ASSERT( write_func );
	    newops = op_call( newops, (void *)write_func );
	    // Restore the clobbered registers.
	    newops = pop_reg( newops, OP_REG_EDX );
	    newops = pop_reg( newops, OP_REG_ECX );
	    newops = pop_reg( newops, OP_REG_EAX );
	    break;
	case 0xa3: // Move EAX to moffs32
	    memcpy( suffixes, &opstream[1], sizeof(suffixes) );
	
	    // Preserve according to the C calling conventions.
	    newops = push_reg( newops, OP_REG_EAX );
	    newops = push_reg( newops, OP_REG_ECX );
	    newops = push_reg( newops, OP_REG_EDX );
	    
	    // We want the source in eax, and the target in edx.  The
	    // target is the device memory address.
	    newops = set_reg ( newops, OP_REG_EDX, * (u32_t *) suffixes );
	    
	    // Call the write function.
	    ASSERT( write_func );
	    newops = op_call( newops, (void *)write_func );
	    // Restore the clobbered registers.
	    newops = pop_reg( newops, OP_REG_EDX );
	    newops = pop_reg( newops, OP_REG_ECX );
	    newops = pop_reg( newops, OP_REG_EAX );
	    
	    break;
	case 0xa1: // MOV moffs32 to EAX
	    // Convert the MOV instruction into a load immediate.
	    newops = mov_imm32_to_reg( newops, *(u32_t *)&newops[1] /*moffs32*/,
		    OP_REG_EAX );
	    // Preserve.
	    newops = push_reg( newops, OP_REG_ECX );
	    newops = push_reg( newops, OP_REG_EDX );
	    // Execute the read function.
	    //newops = set_reg ( newops, OP_REG_EAX, * (u32_t *) suffixes );
	    ASSERT( read_func );
	    newops = op_call( newops, (void *)read_func );
	    // Restore.  The return value is in EAX, as desired.
	    newops = pop_reg( newops, OP_REG_EDX );
	    newops = pop_reg( newops, OP_REG_ECX );
	    break;
	case 0x8b: // Move r/m32 to r32
	    modrm.x.raw = opstream[1];
	    memcpy( suffixes, &opstream[2], sizeof(suffixes) );
	    // Preserve according to the C calling conventions.
	    if( modrm.get_reg() != OP_REG_EAX )
		newops = push_reg( newops, OP_REG_EAX );
	    if( modrm.get_reg() != OP_REG_ECX )
		newops = push_reg( newops, OP_REG_ECX );
	    if( modrm.get_reg() != OP_REG_EDX )
		newops = push_reg( newops, OP_REG_EDX );
	    // Build a pointer to the device memory.
	    newops = lea_modrm( newops, OP_REG_EAX, modrm, suffixes );
	    // Execute the read function.
	    ASSERT( read_func );
	    newops = op_call( newops, (void *)read_func );
	    // Return value is in EAX.  Restore the preserved registers
	    // without clobbering the return value.
	    if( modrm.get_reg() == OP_REG_EDX )
		newops = mov_reg_to_reg( newops, OP_REG_EAX, OP_REG_EDX );
	    else
		newops = pop_reg( newops, OP_REG_EDX );
	    if( modrm.get_reg() == OP_REG_ECX )
		newops = mov_reg_to_reg( newops, OP_REG_EAX, OP_REG_ECX );
	    else
		newops = pop_reg( newops, OP_REG_ECX );
	    if( (modrm.get_reg() != OP_REG_EAX) && (modrm.get_reg() != OP_REG_ECX) && (modrm.get_reg() != OP_REG_EDX) )
		newops = mov_reg_to_reg( newops, OP_REG_EAX, modrm.get_reg() );
	    if( modrm.get_reg() != OP_REG_EAX )
		newops = pop_reg( newops, OP_REG_EAX );
	    break;
	case 0x87: // XCHG r32, r/m32
	    modrm.x.raw = opstream[1];
	    memcpy( suffixes, &opstream[2], sizeof(suffixes) );
	    if( modrm.get_reg() != OP_REG_EAX )
		newops = push_reg( newops, OP_REG_EAX );
	    if( modrm.get_reg() != OP_REG_ECX )
		newops = push_reg( newops, OP_REG_ECX );
	    if( modrm.get_reg() != OP_REG_EDX )
		newops = push_reg( newops, OP_REG_EDX );
	    // We want the source in eax, and the target in edx.  The
	    // target is the device memory address.
	    if( modrm.get_reg() == OP_REG_EAX )
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    else if( modrm.get_reg() != OP_REG_EDX ) {
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, modrm.get_reg(), OP_REG_EAX );
	    }
	    else if( modrm.get_rm() == 0 ) {
	     	// The source is in EDX, and the target is in EAX.  Store the 
		// src temporarily in ECX.
    		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_ECX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
		newops = mov_reg_to_reg( newops, OP_REG_ECX, OP_REG_EAX );
	    }
	    else {
		// The source is in EDX, but the target is not in EAX.
		newops = mov_reg_to_reg( newops, OP_REG_EDX, OP_REG_EAX );
		newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    }
	    // Execute the read function.
	    ASSERT( xchg_func );
	    newops = op_call( newops, (void *)xchg_func );
	    // Return value is in EAX.  Restore the preserved registers
	    // without clobbering the return value.
	    if( modrm.get_reg() == OP_REG_EDX )
		newops = mov_reg_to_reg( newops, OP_REG_EAX, OP_REG_EDX );
	    else
		newops = pop_reg( newops, OP_REG_EDX );
	    if( modrm.get_reg() == OP_REG_ECX )
		newops = mov_reg_to_reg( newops, OP_REG_EAX, OP_REG_ECX );
	    else
		newops = pop_reg( newops, OP_REG_ECX );
	    if( (modrm.get_reg() != OP_REG_EAX) && (modrm.get_reg() != OP_REG_ECX) && (modrm.get_reg() != OP_REG_EDX) )
		newops = mov_reg_to_reg( newops, OP_REG_EAX, modrm.get_reg() );
	    if( modrm.get_reg() != OP_REG_EAX )
		newops = pop_reg( newops, OP_REG_EAX );
	    break;

	default:
	    printf( "Unsupported device patchup @ %x\n", opstream);
	    return false;
    }

    if( newops > opstream_end ) {
	printf( "Insufficient space for the afterburner instructions, @ %x need %d bytes\n",
		opstream, (newops - opstream_end));
	return false;
    }
    return true;
}

bool
arch_apply_device_patchups( patchup_info_t *patchups, word_t total, 
	word_t vaddr_offset, word_t read_func, word_t write_func, 
	word_t xchg_func, word_t or_func, word_t and_func )
{
    for( word_t i = 0; i < total; i++ )
	apply_device_patchup( (u8_t *)(patchups[i].start - vaddr_offset),
		(u8_t *)(patchups[i].end - vaddr_offset), 
		read_func, write_func, xchg_func, or_func, and_func );

    dprintf(debug_patchup, "Total device patchups: %d\n", total);
    return true;
}

static bool
apply_bitop_patchup( u8_t *opstream, u8_t *opstream_end, 
	word_t clear_bit_func, word_t set_bit_func )
{
    u8_t *newops = opstream;
    ia32_modrm_t modrm;
    u8_t suffixes[6];
    
    ASSERT( !set_bit_func );
    stack_offset = 0;

    // May have a lock prefix 
    if( opstream[0] == prefix_lock ) 
	opstream++;

    // Bitop instructions are two-byte
    if( opstream[0] != 0x0f ) {
	printf( "Unsupported instruction rewrite @ %x\n", opstream);
	return false;
    }
    opstream++;

    // Note: we assume that the bit is specified in %eax, and that
    // the value of the CF flag is to be left in %eax.

    switch( opstream[0] ) {
	case 0xb3: // BTR r32, r/m32
	    modrm.x.raw = opstream[1];
	    memcpy( suffixes, &opstream[2], sizeof(suffixes) );
	    ASSERT( modrm.get_reg() == OP_REG_EAX );
	    // Preserve ECX and EDX.
	    newops = push_reg( newops, OP_REG_ECX );
	    newops = push_reg( newops, OP_REG_EDX );
	    // Build a pointer to the device memory.
	    newops = lea_modrm( newops, OP_REG_EDX, modrm, suffixes );
	    // Execute the read function.
	    ASSERT( clear_bit_func );
	    newops = op_call( newops, (void *)clear_bit_func );
	    // Restore ECX and EDX.
	    newops = pop_reg( newops, OP_REG_EDX );
	    newops = pop_reg( newops, OP_REG_ECX );
	    break;
	default:
	    printf( "Unsupported bitop patchup @ %x\n", opstream);
	    return false;
    }

    if( newops > opstream_end ) {
	printf( "Insufficient space for the afterburner instructions, @ %x need %d bytes\n",
		opstream, (newops - opstream_end));
	return false;
    }
    return true;
}

bool
arch_apply_bitop_patchups( patchup_info_t *patchups, word_t total, 
	word_t vaddr_offset, word_t clear_bit_func, word_t set_bit_func )
{
    for( word_t i = 0; i < total; i++ )
	apply_bitop_patchup( (u8_t *)(patchups[i].start - vaddr_offset),
		(u8_t *)(patchups[i].end - vaddr_offset), 
		clear_bit_func, set_bit_func );
 
    dprintf(debug_patchup, "Total device patchups: %d\n", total);
    return true;
}



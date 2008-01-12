/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     amd64/rewrite_stackless.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#ifndef __AMD64__REWRITE_STACKLESS_H__
#define __AMD64__REWRITE_STACKLESS_H__

#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)

#include INC_ARCH(ops.h)
#include INC_ARCH(instr.h)
#include INC_ARCH(intlogic.h)



extern "C" void burn_wrmsr(void);
extern "C" void burn_rdmsr(void);
extern "C" void burn_interruptible_hlt(void);
extern "C" void burn_out(void);
extern "C" void burn_cpuid(void);
extern "C" void burn_in(void);
extern "C" void burn_int(void);
extern "C" void burn_iret(void);
extern "C" void burn_lret(void);
extern "C" void burn_lidt(void);
extern "C" void burn_lgdt(void);
extern "C" void burn_invlpg(void);
extern "C" void burn_lldt(void);
extern "C" void burn_ltr(void);
extern "C" void burn_str(void);
extern "C" void burn_clts(void);
extern "C" void burn_cli(void);
extern "C" void burn_sti(void);
extern "C" void burn_deliver_interrupt(void);
extern "C" void burn_popf(void);
extern "C" void burn_pushf(void);
extern "C" void burn_write_cr0(void);
extern "C" void burn_write_cr2(void);
extern "C" void burn_write_cr3(void);
extern "C" void burn_write_cr4(void);
extern "C" void burn_read_cr(void);
extern "C" void burn_read_cr0(void);
extern "C" void burn_read_cr2(void);
extern "C" void burn_read_cr3(void);
extern "C" void burn_read_cr4(void);
extern "C" void burn_write_dr(void);
extern "C" void burn_read_dr(void);
extern "C" void burn_ud2(void);
extern "C" void burn_unimplemented(void);

extern "C" void burn_write_cs(void);
extern "C" void burn_write_ds(void);
extern "C" void burn_write_es(void);
extern "C" void burn_write_fs(void);
extern "C" void burn_write_gs(void);
extern "C" void burn_write_ss(void);
extern "C" void burn_read_cs(void);
extern "C" void burn_read_ds(void);
extern "C" void burn_read_es(void);
extern "C" void burn_read_fs(void);
extern "C" void burn_read_gs(void);
extern "C" void burn_read_ss(void);
extern "C" void burn_lss(void);
extern "C" void burn_invd(void);
extern "C" void burn_wbinvd(void);

extern "C" void burn_mov_tofsofs(void);
extern "C" void burn_mov_fromfsofs(void);
extern "C" void burn_mov_tofsofs_eax(void);
extern "C" void burn_mov_togsofs(void);
extern "C" void burn_mov_fromgsofs(void);

extern "C" void device_8259_0x20_in(void);
extern "C" void device_8259_0x21_in(void);
extern "C" void device_8259_0xa0_in(void);
extern "C" void device_8259_0xa1_in(void);
extern "C" void device_8259_0x20_out(void);
extern "C" void device_8259_0x21_out(void);
extern "C" void device_8259_0xa0_out(void);
extern "C" void device_8259_0xa1_out(void);


static word_t curr_virt_addr;
static word_t stack_offset;

enum prefix_e {
    prefix_lock=0xf0, prefix_repne=0xf2, prefix_rep=0xf3,
    prefix_operand_size=0x66, prefix_address_size=0x67,
    prefix_cs=0x2e, prefix_ss=0x36, prefix_ds=0x3e, prefix_es=0x26,
    prefix_fs=0x64, prefix_gs=0x65,
};

typedef void (*burn_func_t)();

static inline void update_remain( word_t &remain, u8_t *nop_start, u8_t *end )
{
    if( end >= nop_start ) {
	word_t diff = (word_t)end - (word_t)nop_start;
	remain = min( remain, diff );
    }
}

UNUSED static u8_t *
and_eax_immediate( u8_t *newops, word_t immediate )
{
    newops[0] = 0x25;
    *(u64_t *)&newops[1] = immediate;
    return &newops[5];
}

UNUSED static u8_t *
and_reg_offset_immediate( u8_t *newops, word_t reg, int offset, 
	word_t immediate )
{
    *newops = 0x81;	// AND r/m32, imm32
    newops++;

    amd64_modrm_t modrm;
    modrm.x.fields.reg = 4;
    modrm.x.fields.rm = reg;

    if( (offset >= -128) && (offset <= 127) )
	modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp8;
    else
	modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp;

    *newops = modrm.x.raw;
    newops++;

    if( modrm.x.fields.rm == OP_REG_ESP ) {
	amd64_sib_t sib;
	sib.x.fields.base = OP_REG_ESP;
	sib.x.fields.index = 4;
	sib.x.fields.scale = 0;
	*newops = sib.x.raw;
	newops++;
    }

    if( modrm.x.fields.mod == amd64_modrm_t::mode_indirect_disp8 ) {
	*newops = (u8_t)offset;
	newops++;
    }
    else {
	*(u64_t *)newops = offset;
	newops += sizeof(u64_t);
    }

    *(u64_t *)newops = immediate;
    newops += sizeof(u64_t);
    return newops;
}

UNUSED static u8_t *
or_reg_offset_reg( u8_t *newops, word_t src_reg, word_t dst_reg, int dst_offset )
{
    *newops = 0x09;	// Or into r/m32 with r32
    newops++;

    amd64_modrm_t modrm;
    modrm.x.fields.reg = src_reg;
    modrm.x.fields.rm = dst_reg;

    if( (dst_offset >= -128) && (dst_offset <= 127) )
	modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp8;
    else
	modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp;

    *newops = modrm.x.raw;
    newops++;

    if( modrm.x.fields.rm == OP_REG_ESP ) {
	amd64_sib_t sib;
	sib.x.fields.base = OP_REG_ESP;
	sib.x.fields.index = 4;
	sib.x.fields.scale = 0;
	*newops = sib.x.raw;
	newops++;
    }

    if( modrm.x.fields.mod == amd64_modrm_t::mode_indirect_disp8 ) {
	*newops = (u8_t)dst_offset;
	newops++;
    }
    else {
	*(u64_t *)newops = dst_offset;
	newops += sizeof(u64_t);
    }

    return newops;
}

UNUSED static u8_t *
mov_regoffset_to_reg( u8_t *newops, word_t src_reg, int offset, 
	word_t dst_reg )
{
    newops[0] = 0x8b;	// Move r/m32 to r32

    amd64_modrm_t modrm;
    modrm.x.fields.reg = dst_reg;
    modrm.x.fields.rm = src_reg;

    if( (offset >= -128) && (offset <= 127) ) {
	modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp8;
	newops[1] = modrm.x.raw;
	newops[2] = offset;
	return &newops[3];
    }

    modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp;
    newops[1] = modrm.x.raw;
    *(u64_t *)&newops[2] = offset;
    return &newops[6];
}

UNUSED static u8_t * 
btr_regoffset_immediate( u8_t *newops, word_t reg, int offset, u8_t immediate ) 
{
    newops[0] = 0x0f;
    newops[1] = 0xba;

    amd64_modrm_t modrm;
    modrm.x.fields.reg = 6;
    modrm.x.fields.rm = reg;

    if( (offset >= -128) && (offset <= 127) ) {
	modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp8;
	newops[2] = modrm.x.raw;
	newops[3] = offset;
	newops[4] = immediate;
	return &newops[5];
    }

    modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp;
    newops[2] = modrm.x.raw;
    *(u64_t *)&newops[3] = offset;
    newops[7] = immediate;
    return &newops[8];
}


UNUSED static u8_t *
btr_mem32_immediate( u8_t *newops, word_t address, u8_t immediate )
{
    newops[0] = 0x0f;
    newops[1] = 0xba;

    amd64_modrm_t modrm;
    modrm.x.fields.reg = 6; // Op-code specific.
    modrm.x.fields.rm = 5;  // Displacement 32
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    newops[2] = modrm.x.raw;

    *(u64_t *)&newops[3] = address;
    newops[7] = immediate;

    return &newops[8];
}

UNUSED static u8_t *
bts_mem32_immediate( u8_t *newops, word_t address, u8_t immediate )
{
    newops[0] = 0x0f;
    newops[1] = 0xba;

    amd64_modrm_t modrm;
    modrm.x.fields.reg = 5; // Op-code specific.
    modrm.x.fields.rm = 5;  // Displacement 32
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    newops[2] = modrm.x.raw;

    *(u64_t *)&newops[3] = address;
    newops[7] = immediate;

    return &newops[8];
}

UNUSED static u8_t *
push_mem32( u8_t *newops, word_t address )
{
    newops[0] = 0xff;	// Push r/m32

    amd64_modrm_t modrm;
    modrm.x.fields.reg = 6; // Op-code specific.
    modrm.x.fields.rm = 5;  // Displacement 32
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    newops[1] = modrm.x.raw;

    *(u64_t *)&newops[2] = address;

    stack_offset += sizeof(word_t);
    return &newops[6];
}

UNUSED static u8_t *
pop_mem32( u8_t *newops, word_t address )
{
    newops[0] = 0x8f;	// Push r/m32

    amd64_modrm_t modrm;
    modrm.x.fields.reg = 0; // Op-code specific.
    modrm.x.fields.rm = 5;  // Displacement 32
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    newops[1] = modrm.x.raw;

    *(u64_t *)&newops[2] = address;

    stack_offset -= sizeof(word_t);
    return &newops[6];
}

UNUSED static u8_t *
mov_mem32_to_reg( u8_t *newops, word_t address, word_t reg )
{
    newops[0] = 0x8b;	// Move r/m32 to r32

    amd64_modrm_t modrm;
    modrm.x.fields.reg = reg;
    modrm.x.fields.rm = 5;	// Displacement 32
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    newops[1] = modrm.x.raw;

    *(u64_t *)&newops[2] = address;

    return &newops[6];
}

UNUSED static u8_t *
mov_imm32_to_reg( u8_t *newops, word_t imm32, word_t reg )
{
    newops[0] = 0xc7;	// Move imm32 to r/m32

    amd64_modrm_t modrm;
    modrm.x.fields.reg = reg;
    modrm.x.fields.mod = amd64_modrm_t::mode_reg;
    modrm.x.fields.rm = 0;	// Immediate
    newops[1] = modrm.x.raw;

    *(u64_t *)&newops[2] = imm32;

    return &newops[6];
}

UNUSED static u8_t *
mov_reg_to_mem32( u8_t *newops, word_t reg, word_t address )
{
    newops[0] = 0x89;	// Move r32 to r/m32.

    amd64_modrm_t modrm;
    modrm.x.fields.reg = reg;
    modrm.x.fields.rm = 5;	// Displacement 32
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    newops[1] = modrm.x.raw;

    *(u64_t *)&newops[2] = address;

    return &newops[6];
}

static u8_t *
mov_reg_to_reg( u8_t *newops, word_t src_reg, word_t dst_reg )
{
    newops[0] = 0x89; // Move r32 to r/m32.

    amd64_modrm_t modrm;
    modrm.x.fields.reg = src_reg;
    modrm.x.fields.rm = dst_reg;
    modrm.x.fields.mod = amd64_modrm_t::mode_reg;
    newops[1] = modrm.x.raw;
    return &newops[2];
}

UNUSED static u8_t *
mov_segoffset_eax( u8_t *newops, prefix_e seg_prefix, u64_t offset )
{
    if( seg_prefix != prefix_ds ) {
	newops[0] = seg_prefix;
	newops++;
    }
    newops[0] = 0xa1;	// Move word at (seg:offset) to EAX
    *(u64_t *)&newops[1] = offset;

    return &newops[5];
}

UNUSED static u8_t *
clean_stack( u8_t *opstream, s8_t bytes )
{
    amd64_modrm_t modrm;

    opstream[0] = 0x83;		// add imm8 to r/m32
    modrm.x.fields.reg = 0;	// opcode specific
    modrm.x.fields.mod = amd64_modrm_t::mode_reg;
    modrm.x.fields.rm = 4;	// %esp
    opstream[1] = modrm.x.raw;
    opstream[2] = bytes;

    return &opstream[3];
}

UNUSED static u8_t *
push_byte(u8_t *opstream, u8_t value) 
{
    opstream[0] = OP_PUSH_IMM8;
    opstream[1] = value;
    stack_offset += sizeof(word_t);
    return &opstream[2];
}

UNUSED static u8_t *
push_word(u8_t *opstream, u64_t value) 
{
    opstream[0] = OP_PUSH_IMM32;
    * ((u64_t*)&opstream[1]) = value;
    stack_offset += sizeof(word_t);
    return &opstream[5];
}


UNUSED static u8_t *
insert_operand_size_prefix( u8_t *newops )
{
    *newops = prefix_operand_size;
    newops++;
    return newops;
}

UNUSED static u8_t *
append_modrm( u8_t *newops, amd64_modrm_t modrm, u8_t *suffixes )
/* If any of the modrm suffixes are relative to the stack, we have to adjust to
 * compensate for data that we have pushed to the stack ourselves (since the
 * compiler was unaware of the data that we'd push to the stack when it
 * generated the modrm).  The stack_offset holds the number of bytes that we
 * have pushed on the stack.
 */
{
    amd64_sib_t sib; bool esp_relative = false;

    if( modrm.get_mode() == amd64_modrm_t::mode_reg )
	return newops;  // No suffixes.

    // Write to the new instruction stream any SIB and displacement info.

    if( modrm.get_rm() == 4 ) { // SIB byte
	sib.x.raw = *suffixes;
	esp_relative = sib.get_base() == amd64_sib_t::base_esp;
	*newops = sib.x.raw;
	newops++;
	suffixes++;
    }
    if( modrm.get_mode() == amd64_modrm_t::mode_indirect ) {
	if( modrm.get_rm() == 5 ) {	// 32-bit displacement
	    *(u64_t *)newops = *(u64_t *)suffixes;

	    if (esp_relative)
		*(u64_t *)newops  += stack_offset;
    
	    newops += 4;
	    suffixes += 4;
	    
	}
	else if( (modrm.get_rm() == 4) && sib.is_no_base() ) {
	    // 32-bit displacement
	    *(u64_t *)newops = *(u64_t *)suffixes;
	    
	    if (esp_relative)
		*(u64_t *)newops  += stack_offset;
	    newops += 4;
	    suffixes += 4;
	}
	else if (esp_relative)
	{
	    modrm.x.fields.mod = amd64_modrm_t::mode_indirect_disp8;
	    newops[-2] = modrm.x.raw;
	    *newops = stack_offset;
	    newops++;
	    suffixes++;
	}

    }
    else if( modrm.get_mode() == amd64_modrm_t::mode_indirect_disp8 ) {
	*newops = *suffixes;
	if( esp_relative )
	    *newops += stack_offset;
	newops++;
	suffixes++;
    }
    else if( modrm.get_mode() == amd64_modrm_t::mode_indirect_disp ) {
	*(u64_t *)newops = *(u64_t *)suffixes;
	if( esp_relative )
	    *(u64_t *)newops += stack_offset;
	newops += 4;
	suffixes += 4;
    }
    else
	PANIC( "Unhandled modrm format." );

    return newops;
}

UNUSED static u8_t *
push_modrm( u8_t *newops, amd64_modrm_t modrm, u8_t *suffixes )
{
    newops[0] = 0xff;		// The PUSH instruction.
    modrm.x.fields.reg = 6;	// The PUSH extended opcode.
    newops[1] = modrm.x.raw;

    newops += 2;
    stack_offset += sizeof(word_t);
    return append_modrm( newops, modrm, suffixes );
}

UNUSED static u8_t *
pop_modrm( u8_t *newops, amd64_modrm_t modrm, u8_t *suffixes )
{
    newops[0] = 0x8f;		// The POP instruction.
    modrm.x.fields.reg = 0;	// The POP extended opcode.
    newops[1] = modrm.x.raw;

    newops += 2;
    stack_offset -= sizeof(word_t);
    return append_modrm( newops, modrm, suffixes );
}

UNUSED static u8_t *
mov_to_modrm( u8_t *newops, u8_t src_reg, amd64_modrm_t modrm, u8_t *suffixes )
{
    newops[0] = 0x89;			// MOV r/m,r
    modrm.x.fields.reg = src_reg;
    newops[1] = modrm.x.raw;

    newops += 2;
    return append_modrm( newops, modrm, suffixes );
}

UNUSED static  u8_t *
mov_from_modrm( u8_t *newops, u8_t dst_reg, amd64_modrm_t modrm, u8_t *suffixes )
{
    newops[0] = 0x8b;			// MOV r, r/m
    modrm.x.fields.reg = dst_reg;
    newops[1] = modrm.x.raw;

    newops += 2;
    return append_modrm( newops, modrm, suffixes );
}


UNUSED static u8_t *
lea_modrm( u8_t *newops, u8_t dst_reg, amd64_modrm_t modrm, u8_t *suffixes )
{
    newops[0] = 0x8d;	// Create a lea instruction.
    modrm.x.fields.reg = dst_reg;
    newops[1] = modrm.x.raw;

    newops += 2;
    return append_modrm( newops, modrm, suffixes );
}

UNUSED static u8_t *
movzx_modrm( u8_t *newops, u8_t dst_reg, amd64_modrm_t modrm, u8_t *suffixes )
{
    newops[0] = 0x0f;
    newops[1] = 0xb7;
    modrm.x.fields.reg = dst_reg;
    newops[2] = modrm.x.raw;

    newops += 3;
    return append_modrm( newops, modrm, suffixes );
}

UNUSED static u8_t *
cmp_mem_imm8( u8_t *opstream, word_t mem_addr, u8_t imm8 )
{
    opstream[0] = 0x83;

    amd64_modrm_t modrm;
    modrm.x.fields.mod = amd64_modrm_t::mode_indirect;
    modrm.x.fields.rm = 5;	// disp32
    modrm.x.fields.reg = 7;	// CMP r/m32, imm8

    opstream[1] = modrm.x.raw;
    
    *((word_t *)&opstream[2]) = mem_addr;
    opstream[6] = imm8;

    return &opstream[7];
}

UNUSED static u8_t *
push_reg(u8_t *opstream, u8_t regname) 
{
    opstream[0] = OP_PUSH_REG + regname;
    stack_offset += sizeof(word_t);
    return &opstream[1];
}

UNUSED static u8_t *
pop_reg( u8_t *newops, u8_t regname )
{
    newops[0] = OP_POP_REG + regname;
    stack_offset -= sizeof(word_t);
    return &newops[1];
}

UNUSED static u8_t *
op_popf( u8_t *newops )
{
    newops[0] = OP_POPF;
    return &newops[1];
}

UNUSED static u8_t *
op_pushf( u8_t *newops )
{
    newops[0] = OP_PUSHF;
    return &newops[1];
}

UNUSED static u8_t *
op_jmp(u8_t *opstream, void *func)
{
	opstream[0] = OP_JMP_REL32;
	* ((s64_t*)&opstream[1]) = ((s64_t) func - (((s64_t) &opstream[5] + (s64_t) curr_virt_addr)));
	return &opstream[5];
}

UNUSED static u8_t *
jmp_short_carry( u8_t *opstream, word_t target)
{
    u8_t *next = &opstream[2];
    opstream[0] = OP_JMP_NEAR_CARRY;
    opstream[1] = target - word_t(next);
    return next;
}

UNUSED static u8_t *
jmp_short_zero( u8_t *opstream, word_t target)
{
    u8_t *next = &opstream[2];
    opstream[0] = OP_JMP_NEAR_ZERO;
    opstream[1] = target - word_t(next);
    return next;
}

UNUSED static u8_t *
op_call(u8_t *opstream, void *func)
{
	opstream[0] = OP_CALL_REL32;
	* ((s64_t*)&opstream[1]) = ((s64_t) func - (((s64_t) &opstream[5] + (s64_t) curr_virt_addr)));
	return &opstream[5];
}

UNUSED static u8_t *
op_lcall(u8_t *opstream, void *func)
{
    opstream[0] = OP_LCALL_IMM;
    u16_t cs;
    __asm__ __volatile__ ("mov %%cs, %0" : "=r"(cs));
    *((u16_t *)&opstream[5]) = cs;
    *((word_t *)&opstream[1]) = word_t(func);
    return &opstream[7];
}

UNUSED static u8_t *
push_ip( u8_t *opstream, word_t next_instr )
{
    // Untested
    return push_mem32( opstream, next_instr + curr_virt_addr );
}

UNUSED static u8_t *
set_reg(u8_t *opstream, int reg, u64_t value)
{
	opstream[0] = OP_MOV_IMM32 + reg;
	* ((u64_t*)&opstream[1]) = (u64_t) value;
	return &opstream[5];
}

#if defined(CONFIG_CALLOUT_VCPULOCAL)
#include INC_WEDGE(rewrite_stackless.h)

#else

static inline u8_t*newops_pushf(u8_t *newops) 
{
    newops = push_mem32( newops, (word_t)&get_cpu().flags.x.raw );
    return newops;
}

static inline u8_t*newops_cli(u8_t *newops)
{
    newops = btr_mem32_immediate(newops, (word_t)&get_cpu().flags.x.raw, 9);
    return newops;
}

static u8_t*newops_pushfs(u8_t *newops)
{
    newops = push_mem32( newops, (word_t)&get_cpu().fs );
    return newops;
}

static u8_t*newops_popfs(u8_t *newops)
{
    newops = pop_mem32( newops, (word_t)&get_cpu().fs );
    return newops;
}

static u8_t*newops_pushgs(u8_t *newops)
{
    newops = push_mem32( newops, (word_t)&get_cpu().gs );
    return newops;
}

static u8_t*newops_popgs(u8_t *newops)
{	
    newops = pop_mem32( newops, (word_t)&get_cpu().gs );
    return newops;
}

static inline u8_t*newops_pushcs(u8_t *newops)
{
    newops = push_mem32( newops, (word_t)&get_cpu().cs );
    return newops;
}
static inline u8_t*newops_pushss(u8_t *newops)
{
    newops = push_mem32( newops, (word_t)&get_cpu().ss );
    return newops;
}

static inline u8_t*newops_popss(u8_t *newops)
{
    newops = pop_mem32( newops, (word_t)&get_cpu().ss );
    return newops;
}
static inline u8_t*newops_pushds(u8_t *newops)
{
    newops = push_mem32( newops, (word_t)&get_cpu().ds );
return newops;
}

static inline u8_t*newops_popds(u8_t *newops)
{
    newops = pop_mem32( newops, (word_t)&get_cpu().ds );
    return newops;
}

static inline u8_t*newops_pushes(u8_t *newops)
{
    newops = push_mem32( newops, (word_t)&get_cpu().es );
    return newops;
}

static inline u8_t*newops_popes(u8_t *newops)
{
    newops = pop_mem32( newops, (word_t)&get_cpu().es );
    return newops;
}

static inline u8_t*newops_mov_toseg(u8_t *newops, amd64_modrm_t modrm, u8_t *suffixes) 
{
    if( modrm.is_register_mode() ) 
    {
	switch( modrm.get_reg() ) 
	{
	    case 0:
		newops = mov_reg_to_mem32( newops, modrm.get_rm(),
			(word_t)&get_cpu().es );
		break;
	    case 1:
		newops = mov_reg_to_mem32( newops, modrm.get_rm(),
			(word_t)&get_cpu().cs );
		break;
	    case 2:
		newops = mov_reg_to_mem32( newops, modrm.get_rm(),
			(word_t)&get_cpu().ss );
		break;
	    case 3:
		newops = mov_reg_to_mem32( newops, modrm.get_rm(),
			(word_t)&get_cpu().ds );
		break;
	    case 4:
		newops = mov_reg_to_mem32( newops, modrm.get_rm(),
			(word_t)&get_cpu().fs );
		break;
	    case 5:
		newops = mov_reg_to_mem32( newops, modrm.get_rm(),
			(word_t)&get_cpu().gs );
		break;
	    default:
		con << "unknown segment @ " << (void *)newops << '\n';
		DEBUGGER_ENTER();
		break;
	}
    }
    else 
    {
	switch( modrm.get_reg() ) {
	    case 0:
		newops = push_modrm( newops, modrm, suffixes );
		newops = pop_mem32( newops, (word_t)&get_cpu().es);
		break;
	    case 1:
		newops = push_modrm( newops, modrm, suffixes );
		newops = pop_mem32( newops, (word_t)&get_cpu().cs);
		break;
	    case 2:
		newops = push_modrm( newops, modrm, suffixes );
		newops = pop_mem32( newops, (word_t)&get_cpu().ss);
		break;
	    case 3:
		newops = push_modrm( newops, modrm, suffixes );
		newops = pop_mem32( newops, (word_t)&get_cpu().ds);
		break;
	    case 4:
		newops = push_modrm( newops, modrm, suffixes );
		newops = pop_mem32( newops, (word_t)&get_cpu().fs);
		break;
	    case 5:
		newops = push_modrm( newops, modrm, suffixes );
		newops = pop_mem32( newops, (word_t)&get_cpu().gs);
		break;
	    default:
		con << "unknown segment @ " << (void *)newops << '\n';
		DEBUGGER_ENTER();
		break;
	}
    }
    return newops;
}


static inline u8_t*newops_movfromseg(u8_t *newops, amd64_modrm_t modrm, u8_t *suffixes)
{
    if( modrm.is_register_mode() ) {
	switch( modrm.get_reg() ) {
	    case 0:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().es, modrm.get_rm() );
		break;
	    case 1:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().cs, modrm.get_rm() );
		break;
	    case 2:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().ss, modrm.get_rm() );
		break;
	    case 3:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().ds, modrm.get_rm() );
		break;
	    case 4:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().fs, modrm.get_rm() );
		break;
	    case 5:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().gs, modrm.get_rm() );
		break;
	    default:
		con << "unknown segment @ " << (void *)newops << '\n';
		return false;
	}
    }
    else {
	switch( modrm.get_reg() ) {
	    case 0:
		newops = push_mem32( newops, (word_t)&get_cpu().es);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 1:
		newops = push_mem32( newops, (word_t)&get_cpu().cs);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 2:
		newops = push_mem32( newops, (word_t)&get_cpu().ss);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 3:
		newops = push_mem32( newops, (word_t)&get_cpu().ds);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 4:
		newops = push_mem32( newops, (word_t)&get_cpu().fs);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 5:
		newops = push_mem32( newops, (word_t)&get_cpu().gs);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    default:
		con << "unknown segment @ " << (void *)newops << '\n';
		return false;
	}
    }
    return newops;
}

static u8_t*newops_movfromcreg(u8_t *newops, amd64_modrm_t modrm, u8_t *suffixes)
{	
    if( modrm.is_register_mode() ) 
    {
	switch( modrm.get_reg() ) 
	{
	    case 0:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().cr0, modrm.get_rm() );
		break;
	    case 2:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().cr2, modrm.get_rm() );
		break;
	    case 3:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().cr3, modrm.get_rm() );
		break;
	    case 4:
		newops = mov_mem32_to_reg( newops, 
			(word_t)&get_cpu().cr4, modrm.get_rm() );
		break;
	    default:
		con << "unknown CR @ " << (void *)newops << '\n';
		DEBUGGER_ENTER();
		break;
	}
    }
    else 
    {
	switch( modrm.get_reg() ) {
	    case 0:
		newops = push_mem32( newops, (word_t)&get_cpu().cr0);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 2:
		newops = push_mem32( newops, (word_t)&get_cpu().cr2);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 3:
		newops = push_mem32( newops, (word_t)&get_cpu().cr3);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    case 4:
		newops = push_mem32( newops, (word_t)&get_cpu().cr4);
		newops = pop_modrm( newops, modrm, suffixes );
		break;
	    default:
		con << "unknown CR @ " << (void *)newops << '\n';
		DEBUGGER_ENTER();
		break;
	}
    }
    return newops;
}

#endif /* CONFIG_CALLOUT_VCPULOCAL */


#endif /* !__AMD64__REWRITE_STACKLESS_H__ */

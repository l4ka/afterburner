/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/ia32/instr.h
 * Description:   Support types for IA32 instruction decoding.
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
 * $Id: instr.h,v 1.5 2005/11/17 16:40:59 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__IA32__INSTR_H__
#define __AFTERBURN_WEDGE__INCLUDE__IA32__INSTR_H__

#include INC_ARCH(types.h)

struct ia32_modrm_t {
    enum modes_e {
	mode_indirect=0, mode_indirect_disp8=1,
	mode_indirect_disp=2, mode_reg=3,
    };

    union {
	u8_t raw;
	struct {
	    u8_t rm  : 3;	// Register, or specifies an addressing mode.
	    u8_t reg : 3;	// Register, or opcode extension.
	    u8_t mod : 2;
	} fields;
    } x;

    modes_e get_mode() { return (modes_e)x.fields.mod; }
    u8_t get_rm() { return x.fields.rm; }
    u8_t get_opcode() { return x.fields.reg; }
    bool is_register_mode() { return get_mode() == mode_reg; }

    u8_t get_reg() { return x.fields.reg; }
    u8_t get_dst() { return x.fields.rm; }
};

struct ia32_sib_t {
    enum base_e { base_eax=0, base_ecx=1, base_edx=2, base_ebx=3,
	base_esp=4, base_none_ebp=5, base_esi=6, base_edi=7 };

    union {
	u8_t raw;
	struct {
	    u8_t base : 3;
	    u8_t index : 3;
	    u8_t scale : 2;
	} fields;
    } x;

    base_e get_base() { return (base_e)x.fields.base; }
    bool is_no_base() { return x.fields.base == base_none_ebp; }
};

#endif	/* __AFTERBURN_WEDGE__INCLUDE__IA32__INSTR_H__ */

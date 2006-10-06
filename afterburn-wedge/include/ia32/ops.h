/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/include/ia32/cpu.h
 * Description:   The CPU model, and support types.
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
 * $Id: ops.h,v 1.3 2005/12/16 12:11:27 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__IA32__OPS_H__
#define __AFTERBURN_WEDGE__INCLUDE__IA32__OPS_H__

#define OP_NOP1		0x90

#define OP_2BYTE	0x0f

#define OP_UD2		0x0b

#define OP_WBINVD	0x09
#define OP_INVD		0x08

#define OP_PUSHF	0x9c
#define OP_POPF		0x9d

#define OP_LLTL		0x00
#define OP_LDTL		0x01

#define OP_MOV_IMM32	0xb8
#define OP_MOV_MEM32	0x89
#define OP_MOV_FROM_MEM32 0x8b
#define OP_MOV_TOSEG	0x8e
#define OP_MOV_FROMSEG	0x8c
#define OP_MOV_TOCREG	0x22

#define OP_PUSH_CS	0x0e
#define OP_PUSH_SS	0x16
#define OP_PUSH_DS	0x1e
#define OP_PUSH_ES	0x06
#define OP_PUSH_FS	0xa0
#define OP_PUSH_GS	0xa8

#define OP_POP_DS	0x1f
#define OP_POP_ES	0x07
#define OP_POP_SS	0x17
#define OP_POP_FS	0xa1
#define OP_POP_GS	0xa9

#define OP_MOV_FROMSEG	0x8c
#define OP_MOV_TODREG	0x23

#define OP_MOV_FROMCREG 0x20
#define OP_MOV_FROMDREG 0x21

#define OP_CLTS		0x06
#define OP_STI		0xfb
#define OP_CLI		0xfa

#define OP_CALL_REL32	0xe8
#define OP_LCALL_IMM	0x9a
#define OP_RET		0xc3
#define OP_JMP_REL32	0xe9
#define OP_JMP_FAR	0xea
#define OP_JMP_NEAR_CARRY 0x72
#define OP_JMP_NEAR_ZERO 0x74

#define OP_PUSH_REG	0x50
#define OP_PUSH_IMM8	0x6a
#define OP_PUSH_IMM32	0x68

#define OP_POP_REG	0x58

#define OP_CPUID	0xa2

#define OP_LRET		0xcb
#define OP_IRET		0xcf
#define OP_INT		0xcd
#define OP_INT3		0xcc

#define OP_WRMSR	0x30
#define OP_RDTSC	0x31
#define OP_RDMSR	0x32

#define OP_HLT		0xf4

#define OP_LSS		0xb2

#define OP_OUT		0xe7
#define OP_OUTB		0xe6

#define OP_OUT_DX	0xef
#define OP_OUTB_DX	0xee

#define OP_SIZE_OVERRIDE 0x66

#define OP_IN		0xe5
#define OP_INB		0xe4

#define OP_INB_DX	0xec
#define OP_IN_DX	0xed


/* Register operands */
#define OP_REG_EAX 0
#define OP_REG_ECX 1
#define OP_REG_EDX 2
#define OP_REG_EBX 3
#define OP_REG_ESP 4
#define OP_REG_EBP 5
#define OP_REG_ESI 6
#define OP_REG_EDI 7

#endif /* __AFTERBURN_WEDGE__INCLUDE__IA32__OPS_H__ */

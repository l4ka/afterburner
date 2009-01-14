/*
 * io.h: HVM IO support
 *
 * Copyright (c) 2004, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef __ASM_X86_HVM_IO_H__
#define __ASM_X86_HVM_IO_H__

#include <l4/ia32/arch.h>

#define operand_size(operand)   \
    ((operand >> 24) & 0xFF)

#define operand_index(operand)  \
    ((operand >> 16) & 0xFF)

/* for instruction.operand[].size */
#define BYTE       1
#define WORD       2
#define LONG       4
#define QUAD       8
#define BYTE_64    16

/* for instruction.operand[].flag */
#define REGISTER   0x1
#define MEMORY     0x2
#define IMMEDIATE  0x4

/* for instruction.flags */
#define REPZ       0x1
#define REPNZ      0x2
#define OVERLAP    0x4

/* instruction type */
#define INSTR_PIO   1
#define INSTR_OR    2
#define INSTR_AND   3
#define INSTR_XOR   4
#define INSTR_CMP   5
#define INSTR_MOV   6
#define INSTR_MOVS  7
#define INSTR_MOVZX 8
#define INSTR_MOVSX 9
#define INSTR_STOS  10
#define INSTR_LODS  11
#define INSTR_TEST  12
#define INSTR_BT    13
#define INSTR_XCHG  14
#define INSTR_SUB   15
#define INSTR_ADD   16
#define INSTR_PUSH  17

#define MAX_INST_LEN      15 /* Maximum instruction length = 15 bytes */

/* platform.c / io.c */
void handle_mmio(word_t gpa); 
void hvm_mmio_assist(void);
void hvm_copy_from_guest_phys(void *buf, word_t addr, word_t size);
word_t qemu_is_mmio_area(word_t addr);
long get_reg_value(int size, int index, int seg,L4_IA32_GPRegs_t  *regs);

/* instlen.c */
int hvm_instruction_fetch(unsigned long org_pc, int address_bytes,
                          unsigned char *buf);
struct hvm_io_op {
    unsigned int            instr;      /* instruction */
    unsigned int            flags;
    unsigned long           addr;       /* virt addr for overlap PIO/MMIO */
    struct {
        unsigned int        operand[2]; /* operands */
        unsigned long       immediate;  /* immediate portion */
    };
    L4_IA32_GPRegs_t    *io_context; /* current context */
};

#endif /* __ASM_X86_HVM_IO_H__ */


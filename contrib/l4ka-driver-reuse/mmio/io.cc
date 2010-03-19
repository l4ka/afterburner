/*
 * io.c: Handling I/O and interrupts.
 *
 * Copyright (c) 2004, Intel Corporation.
 * Copyright (c) 2005, International Business Machines Corporation.
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

#include <debug.h>
#include <string.h>
#include <console.h>
#include INC_WEDGE(qemu.h)
#include INC_WEDGE(backend.h)

#include <l4/ia32/arch.h>

#include "io.h"
#include "x86_emulate.h"

extern struct hvm_io_op global_mmio_op;

static inline unsigned long
hvm_get_segment_base( enum x86_segment seg)
{
    return get_vcpu().main_info.mrs.seg_item[seg-L4_CTRLXFER_CSREGS_ID].regs.base;
}

void hvm_copy_from_guest_phys(
     void *buf, word_t addr, word_t size)
{
    word_t paddr;
    word_t psize;
    word_t n = 0;

    //TODO improve performance. currently we copy one byte at the time.

    dprintf(debug_qemu,"copy_from_guest size %d addr %lx \n",addr,size);
    while (n < size)
    {    
	backend_resolve_kaddr(addr + n,paddr,size,psize);
	dprintf(debug_qemu,"copy_from_guest addr %lx, paddr %lx, size %d\n",addr + n, paddr,size);

	*((uint8_t *)buf + n) = *((uint8_t *)paddr);
	n++;
    }
}

void hvm_copy_to_guest_phys(
    word_t addr, void * buf, word_t size)
{
    word_t paddr;
    word_t psize;
    word_t n = 0;

    //TODO improve performance. currently we copy one byte at the time.
    size = 1;
    paddr = 0;

    DEBUGGER_ENTER("TEST");
    dprintf(debug_qemu,"copy_to_guest size %d addr %lx \n",addr,size);

    while (n < size)
    {    
	backend_resolve_kaddr(addr + n,paddr,size,psize);
	dprintf(debug_qemu,"copy_to_guest addr %lx, paddr %lx, size %d\n",addr + n, paddr,size);
	*((uint8_t *)paddr) = *((uint8_t *)buf + n);
	n++;
    }
}

static void set_reg_value (int size, int index, int seg, L4_IA32_GPRegs_t *regs, long value)
{
    switch (size) {
    case BYTE:
        switch (index) {
        case 0:
            regs->eax &= 0xFFFFFF00;
            regs->eax |= (value & 0xFF);
            break;
        case 1:
            regs->ecx &= 0xFFFFFF00;
            regs->ecx |= (value & 0xFF);
            break;
        case 2:
            regs->edx &= 0xFFFFFF00;
            regs->edx |= (value & 0xFF);
            break;
        case 3:
            regs->ebx &= 0xFFFFFF00;
            regs->ebx |= (value & 0xFF);
            break;
        case 4:
            regs->eax &= 0xFFFF00FF;
            regs->eax |= ((value & 0xFF) << 8);
            break;
        case 5:
            regs->ecx &= 0xFFFF00FF;
            regs->ecx |= ((value & 0xFF) << 8);
            break;
        case 6:
            regs->edx &= 0xFFFF00FF;
            regs->edx |= ((value & 0xFF) << 8);
            break;
        case 7:
            regs->ebx &= 0xFFFF00FF;
            regs->ebx |= ((value & 0xFF) << 8);
            break;
        default:
            goto crash;
        }
        break;
    case WORD:
        switch (index) {
        case 0:
            regs->eax &= 0xFFFF0000;
            regs->eax |= (value & 0xFFFF);
            break;
        case 1:
            regs->ecx &= 0xFFFF0000;
            regs->ecx |= (value & 0xFFFF);
            break;
        case 2:
            regs->edx &= 0xFFFF0000;
            regs->edx |= (value & 0xFFFF);
            break;
        case 3:
            regs->ebx &= 0xFFFF0000;
            regs->ebx |= (value & 0xFFFF);
            break;
        case 4:
            regs->esp &= 0xFFFF0000;
            regs->esp |= (value & 0xFFFF);
            break;
        case 5:
            regs->ebp &= 0xFFFF0000;
            regs->ebp |= (value & 0xFFFF);
            break;
        case 6:
            regs->esi &= 0xFFFF0000;
            regs->esi |= (value & 0xFFFF);
            break;
        case 7:
            regs->edi &= 0xFFFF0000;
            regs->edi |= (value & 0xFFFF);
            break;
        default:
            goto crash;
        }
        break;
    case LONG:
        switch (index) {
        case 0:
            regs->eax = value;
            break;
        case 1:
            regs->ecx = value;
            break;
        case 2:
            regs->edx = value;
            break;
        case 3:
            regs->ebx = value;
            break;
        case 4:
            regs->esp = value;
            break;
        case 5:
            regs->ebp = value;
            break;
        case 6:
            regs->esi = value;
            break;
        case 7:
            regs->edi = value;
            break;
        default:
            goto crash;
        }
        break;
    default:
    crash:
        printf("size:%x, index:%x are invalid!\n", size, index);
	DEBUGGER_ENTER("untested");	
    }
}

static inline void set_eflags_CF(int size,
                                 unsigned int instr,
                                 unsigned long result,
                                 unsigned long src,
                                 unsigned long dst,
                                 L4_IA32_GPRegs_t *regs)
{
    unsigned long mask;

    if ( size == BYTE_64 )
        size = BYTE;
    ASSERT(((unsigned long)size <= sizeof(mask)) && (size > 0));

    mask = ~0UL >> (8 * (sizeof(mask) - size));

    if ( instr == INSTR_ADD )
    {
        /* CF=1 <==> result is less than the augend and addend) */
        if ( (result & mask) < (dst & mask) )
        {
            ASSERT((result & mask) < (src & mask));
            regs->eflags |= X86_FLAGS_CF;
        }
    }
    else
    {
        ASSERT( instr == INSTR_CMP || instr == INSTR_SUB );
        if ( (src & mask) > (dst & mask) )
            regs->eflags |= X86_FLAGS_CF;
    }
}

static inline void set_eflags_OF(int size,
                                 unsigned int instr,
                                 unsigned long result,
                                 unsigned long src,
                                 unsigned long dst,
				 L4_IA32_GPRegs_t *regs)
{
    unsigned long mask;

    if ( size == BYTE_64 )
        size = BYTE;
    ASSERT(((unsigned int)size <= sizeof(mask)) && (size > 0));

    mask =  1UL << ((8*size) - 1);

    if ( instr == INSTR_ADD )
    {
        if ((src ^ result) & (dst ^ result) & mask);
            regs->eflags |= X86_FLAGS_OF;
    }
    else
    {
        ASSERT(instr == INSTR_CMP || instr == INSTR_SUB);
        if ((dst ^ src) & (dst ^ result) & mask)
            regs->eflags |= X86_FLAGS_OF;
    }
}

static inline void set_eflags_AF(int size,
                                 unsigned long result,
                                 unsigned long src,
                                 unsigned long dst,
                                 L4_IA32_GPRegs_t *regs)
{
    if ((result ^ src ^ dst) & 0x10)
        regs->eflags |= X86_FLAGS_AF;
}

static inline void set_eflags_ZF(int size, unsigned long result,
                                 L4_IA32_GPRegs_t *regs)
{
    unsigned long mask;

    if ( size == BYTE_64 )
        size = BYTE;
    ASSERT(((unsigned int)size <= sizeof(mask)) && (size > 0));

    mask = ~0UL >> (8 * (sizeof(mask) - size));

    if ((result & mask) == 0)
        regs->eflags |= X86_FLAGS_ZF;
}

static inline void set_eflags_SF(int size, unsigned long result,
                                 L4_IA32_GPRegs_t *regs)
{
    unsigned long mask;

    if ( size == BYTE_64 )
        size = BYTE;
    ASSERT(((unsigned int)size <= sizeof(mask)) && (size > 0));

    mask = 1UL << ((8*size) - 1);

    if (result & mask)
        regs->eflags |= X86_FLAGS_SF;
}

static char parity_table[256] = {
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

static inline void set_eflags_PF(int size, unsigned long result,
                                 L4_IA32_GPRegs_t *regs)
{
    if (parity_table[result & 0xFF])
        regs->eflags |= X86_FLAGS_PF;
}

extern qemu_t qemu;
void hvm_mmio_assist(void)
{
    struct hvm_io_op *mmio_opp = &global_mmio_op;
    L4_IA32_GPRegs_t *regs = mmio_opp->io_context;
    ioreq_t *p = &qemu.s_pages.sio_page.vcpu_iodata[0].vp_ioreq;


    int sign = p->df ? -1 : 1;
    int size = -1, index = -1;
    unsigned long value = 0, result = 0;
    unsigned long src, dst;

    src = mmio_opp->operand[0];
    dst = mmio_opp->operand[1];
    size = operand_size(src);

    switch (mmio_opp->instr) {
    case INSTR_MOV:
        if (dst & REGISTER) {
            index = operand_index(dst);
            set_reg_value(size, index, 0, regs, p->data);
        }
        break;

    case INSTR_MOVZX:
        if (dst & REGISTER) {
            switch (size) {
            case BYTE:
                p->data &= 0xFFULL;
                break;

            case WORD:
                p->data &= 0xFFFFULL;
                break;

            case LONG:
                p->data &= 0xFFFFFFFFULL;
                break;

            default:
                printf("Impossible source operand size of movzx instr: %d\n", size);
		DEBUGGER_ENTER("untested");	
            }
            index = operand_index(dst);
            set_reg_value(operand_size(dst), index, 0, regs, p->data);
        }
        break;

    case INSTR_MOVSX:
        if (dst & REGISTER) {
            switch (size) {
            case BYTE:
                p->data &= 0xFFULL;
                if ( p->data & 0x80ULL )
                    p->data |= 0xFFFFFFFFFFFFFF00ULL;
                break;

            case WORD:
                p->data &= 0xFFFFULL;
                if ( p->data & 0x8000ULL )
                    p->data |= 0xFFFFFFFFFFFF0000ULL;
                break;

            case LONG:
                p->data &= 0xFFFFFFFFULL;
                if ( p->data & 0x80000000ULL )
                    p->data |= 0xFFFFFFFF00000000ULL;
                break;

            default:
                printf("Impossible source operand size of movsx instr: %d\n", size);
		DEBUGGER_ENTER("untested");	
            }
            index = operand_index(dst);
            set_reg_value(operand_size(dst), index, 0, regs, p->data);
        }
        break;

    case INSTR_MOVS:
        sign = p->df ? -1 : 1;

        if (mmio_opp->flags & REPZ)
            regs->ecx -= p->count;

        if ((mmio_opp->flags & OVERLAP) && p->dir == IOREQ_READ) {
            unsigned long addr = mmio_opp->addr;
	    (void)hvm_copy_to_guest_phys(addr, &p->data, p->size);
        }

        regs->esi += sign * p->count * p->size;
        regs->edi += sign * p->count * p->size;

        break;

    case INSTR_STOS:
        sign = p->df ? -1 : 1;
        regs->edi += sign * p->count * p->size;
        if (mmio_opp->flags & REPZ)
            regs->ecx -= p->count;
        break;

    case INSTR_LODS:
        set_reg_value(size, 0, 0, regs, p->data);
        sign = p->df ? -1 : 1;
        regs->esi += sign * p->count * p->size;
        if (mmio_opp->flags & REPZ)
            regs->ecx -= p->count;
        break;

    case INSTR_AND:
        if (src & REGISTER) {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data & value;
        } else if (src & IMMEDIATE) {
            value = mmio_opp->immediate;
            result = (unsigned long) p->data & value;
        } else if (src & MEMORY) {
            index = operand_index(dst);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data & value;
            set_reg_value(size, index, 0, regs, result);
        }

        /*
         * The OF and CF flags are cleared; the SF, ZF, and PF
         * flags are set according to the result. The state of
         * the AF flag is undefined.
         */
        regs->eflags &= ~(X86_FLAGS_CF|X86_FLAGS_PF|
                          X86_FLAGS_ZF|X86_FLAGS_SF|X86_FLAGS_OF);
        set_eflags_ZF(size, result, regs);
        set_eflags_SF(size, result, regs);
        set_eflags_PF(size, result, regs);
        break;

    case INSTR_ADD:
        if (src & REGISTER) {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data + value;
        } else if (src & IMMEDIATE) {
            value = mmio_opp->immediate;
            result = (unsigned long) p->data + value;
        } else if (src & MEMORY) {
            index = operand_index(dst);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data + value;
            set_reg_value(size, index, 0, regs, result);
        }

        /*
         * The CF, OF, SF, ZF, AF, and PF flags are set according
         * to the result
         */
        regs->eflags &= ~(X86_FLAGS_CF|X86_FLAGS_PF|X86_FLAGS_AF|
                          X86_FLAGS_ZF|X86_FLAGS_SF|X86_FLAGS_OF);
        set_eflags_CF(size, mmio_opp->instr, result, value,
                      (unsigned long) p->data, regs);
        set_eflags_OF(size, mmio_opp->instr, result, value,
                      (unsigned long) p->data, regs);
        set_eflags_AF(size, result, value, (unsigned long) p->data, regs);
        set_eflags_ZF(size, result, regs);
        set_eflags_SF(size, result, regs);
        set_eflags_PF(size, result, regs);
        break;

    case INSTR_OR:
        if (src & REGISTER) {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data | value;
        } else if (src & IMMEDIATE) {
            value = mmio_opp->immediate;
            result = (unsigned long) p->data | value;
        } else if (src & MEMORY) {
            index = operand_index(dst);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data | value;
            set_reg_value(size, index, 0, regs, result);
        }

        /*
         * The OF and CF flags are cleared; the SF, ZF, and PF
         * flags are set according to the result. The state of
         * the AF flag is undefined.
         */
        regs->eflags &= ~(X86_FLAGS_CF|X86_FLAGS_PF|
                          X86_FLAGS_ZF|X86_FLAGS_SF|X86_FLAGS_OF);
        set_eflags_ZF(size, result, regs);
        set_eflags_SF(size, result, regs);
        set_eflags_PF(size, result, regs);
        break;

    case INSTR_XOR:
        if (src & REGISTER) {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data ^ value;
        } else if (src & IMMEDIATE) {
            value = mmio_opp->immediate;
            result = (unsigned long) p->data ^ value;
        } else if (src & MEMORY) {
            index = operand_index(dst);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data ^ value;
            set_reg_value(size, index, 0, regs, result);
        }

        /*
         * The OF and CF flags are cleared; the SF, ZF, and PF
         * flags are set according to the result. The state of
         * the AF flag is undefined.
         */
        regs->eflags &= ~(X86_FLAGS_CF|X86_FLAGS_PF|
                          X86_FLAGS_ZF|X86_FLAGS_SF|X86_FLAGS_OF);
        set_eflags_ZF(size, result, regs);
        set_eflags_SF(size, result, regs);
        set_eflags_PF(size, result, regs);
        break;

    case INSTR_CMP:
    case INSTR_SUB:
        if (src & REGISTER) {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
            result = (unsigned long) p->data - value;
        } else if (src & IMMEDIATE) {
            value = mmio_opp->immediate;
            result = (unsigned long) p->data - value;
        } else if (src & MEMORY) {
            index = operand_index(dst);
            value = get_reg_value(size, index, 0, regs);
            result = value - (unsigned long) p->data;
            if ( mmio_opp->instr == INSTR_SUB )
                set_reg_value(size, index, 0, regs, result);
        }

        /*
         * The CF, OF, SF, ZF, AF, and PF flags are set according
         * to the result
         */
        regs->eflags &= ~(X86_FLAGS_CF|X86_FLAGS_PF|X86_FLAGS_AF|
                          X86_FLAGS_ZF|X86_FLAGS_SF|X86_FLAGS_OF);
        if ( src & (REGISTER | IMMEDIATE) )
        {
            set_eflags_CF(size, mmio_opp->instr, result, value,
                          (unsigned long) p->data, regs);
            set_eflags_OF(size, mmio_opp->instr, result, value,
                          (unsigned long) p->data, regs);
        }
        else
        {
            set_eflags_CF(size, mmio_opp->instr, result,
                          (unsigned long) p->data, value, regs);
            set_eflags_OF(size, mmio_opp->instr, result,
                          (unsigned long) p->data, value, regs);
        }
        set_eflags_AF(size, result, value, (unsigned long) p->data, regs);
        set_eflags_ZF(size, result, regs);
        set_eflags_SF(size, result, regs);
        set_eflags_PF(size, result, regs);
        break;

    case INSTR_TEST:
        if (src & REGISTER) {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
        } else if (src & IMMEDIATE) {
            value = mmio_opp->immediate;
        } else if (src & MEMORY) {
            index = operand_index(dst);
            value = get_reg_value(size, index, 0, regs);
        }
        result = (unsigned long) p->data & value;

        /*
         * Sets the SF, ZF, and PF status flags. CF and OF are set to 0
         */
        regs->eflags &= ~(X86_FLAGS_CF|X86_FLAGS_PF|
                          X86_FLAGS_ZF|X86_FLAGS_SF|X86_FLAGS_OF);
        set_eflags_ZF(size, result, regs);
        set_eflags_SF(size, result, regs);
        set_eflags_PF(size, result, regs);
        break;

    case INSTR_BT:
        if ( src & REGISTER )
        {
            index = operand_index(src);
            value = get_reg_value(size, index, 0, regs);
        }
        else if ( src & IMMEDIATE )
            value = mmio_opp->immediate;
        if (p->data & (1 << (value & ((1 << 5) - 1))))
            regs->eflags |= X86_FLAGS_CF;
        else
            regs->eflags &= ~X86_FLAGS_CF;

        break;

    case INSTR_XCHG:
        if (src & REGISTER) {
            index = operand_index(src);
            set_reg_value(size, index, 0, regs, p->data);
        } else {
            index = operand_index(dst);
            set_reg_value(size, index, 0, regs, p->data);
        }
        break;

    case INSTR_PUSH:
        mmio_opp->addr += hvm_get_segment_base(x86_seg_ss);
        {
            unsigned long addr = mmio_opp->addr;
            hvm_copy_to_guest_phys(addr, &p->data, size);
        }
        break;
    }
}

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

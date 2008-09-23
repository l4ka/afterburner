#include INC_ARCH(types.h)
#include <debug.h>
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(qemu_dm.h)
#include INC_WEDGE(qemu_mmio.h)
#include <console.h>
#include <string.h>

#include <l4/ia32/arch.h>

extern qemu_dm_t qemu_dm;

word_t qemu_is_mmio_area(word_t addr)
{
    if((addr >= 0xa0000) && (addr < 0xc0000))
	return true;

    return false;
}

static void resolve_operand_addr(word_t seg,word_t addr, word_t &paddr, word_t instr_len) 
{
    word_t tmp_paddr;
    word_t psize;


    switch(instr_len)
    {
	case 2:
	    if(!backend_async_read_eaddr(seg, addr, tmp_paddr))
		goto error;
	    break;
	case 4:
	    backend_resolve_kaddr(addr,1,tmp_paddr,psize);
	    if(psize != 1)
		goto error;
	    break;
    }
    paddr = tmp_paddr;

    return;

error:
    printf("resolving operand physical addr failed. addr = %lx, instr_len = %d\n",addr, instr_len);
    DEBUGGER_ENTER("Untested");

}

#define DECODE_success  1
#define DECODE_failure  0

#define mk_operand(size_reg, index, seg, flag) \
    (((size_reg) << 24) | ((index) << 16) | ((seg) << 8) | (flag))

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
    paddr = 0;

    DEBUGGER_ENTER("TEST");

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
            regs->eflags |= X86_EFLAGS_CF;
        }
    }
    else
    {
        ASSERT( instr == INSTR_CMP || instr == INSTR_SUB );
        if ( (src & mask) > (dst & mask) )
            regs->eflags |= X86_EFLAGS_CF;
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
            regs->eflags |= X86_EFLAGS_OF;
    }
    else
    {
        ASSERT(instr == INSTR_CMP || instr == INSTR_SUB);
        if ((dst ^ src) & (dst ^ result) & mask)
            regs->eflags |= X86_EFLAGS_OF;
    }
}

static inline void set_eflags_AF(int size,
                                 unsigned long result,
                                 unsigned long src,
                                 unsigned long dst,
                                 L4_IA32_GPRegs_t *regs)
{
    if ((result ^ src ^ dst) & 0x10)
        regs->eflags |= X86_EFLAGS_AF;
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
        regs->eflags |= X86_EFLAGS_ZF;
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
        regs->eflags |= X86_EFLAGS_SF;
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
        regs->eflags |= X86_EFLAGS_PF;
}


#if defined (__i386__)
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
#endif

#if defined (__i386__)
static inline long __get_reg_value(unsigned long reg, int size)
{
    switch ( size ) {
    case WORD:
        return (short)(reg & 0xFFFF);
    case LONG:
        return (int)(reg & 0xFFFFFFFF);
    default:
        printf("Error: (__get_reg_value) Invalid reg size\n");
	DEBUGGER_ENTER("untested");	
    }
}

long get_reg_value(int size, int index, int seg,L4_IA32_GPRegs_t  *regs)
{
    if ( size == BYTE ) {
        switch ( index ) {
        case 0: /* %al */
            return (char)(regs->eax & 0xFF);
        case 1: /* %cl */
            return (char)(regs->ecx & 0xFF);
        case 2: /* %dl */
            return (char)(regs->edx & 0xFF);
        case 3: /* %bl */
            return (char)(regs->ebx & 0xFF);
        case 4: /* %ah */
            return (char)((regs->eax & 0xFF00) >> 8);
        case 5: /* %ch */
            return (char)((regs->ecx & 0xFF00) >> 8);
        case 6: /* %dh */
            return (char)((regs->edx & 0xFF00) >> 8);
        case 7: /* %bh */
            return (char)((regs->ebx & 0xFF00) >> 8);
        default:
            printf("Error: (get_reg_value) Invalid index value\n");
	    DEBUGGER_ENTER("untested");

        }
    }

    switch ( index ) {
    case 0: return __get_reg_value(regs->eax, size);
    case 1: return __get_reg_value(regs->ecx, size);
    case 2: return __get_reg_value(regs->edx, size);
    case 3: return __get_reg_value(regs->ebx, size);
    case 4: return __get_reg_value(regs->esp, size);
    case 5: return __get_reg_value(regs->ebp, size);
    case 6: return __get_reg_value(regs->esi, size);
    case 7: return __get_reg_value(regs->edi, size);
    default:
        printf("Error: (get_reg_value) Invalid index value\n");
	DEBUGGER_ENTER("untested");
	return 0; //satisfy compiler
    }
}
#endif

static inline unsigned long get_immediate(int ad_size, const unsigned char *inst, int op_size)
{
    int mod, reg, rm;
    unsigned long val = 0;
    int i;

    mod = (*inst >> 6) & 3;
    reg = (*inst >> 3) & 7;
    rm = *inst & 7;

    inst++; //skip ModR/M byte
    if ( ad_size != WORD && mod != 3 && rm == 4 ) {
        rm = *inst & 7;
        inst++; //skip SIB byte
    }

    switch ( mod ) {
    case 0:
        if ( ad_size == WORD ) {
            if ( rm == 6 )
                inst = inst + 2; //disp16, skip 2 bytes
        }
        else {
            if ( rm == 5 )
                inst = inst + 4; //disp32, skip 4 bytes
        }
        break;
    case 1:
        inst++; //disp8, skip 1 byte
        break;
    case 2:
        if ( ad_size == WORD )
            inst = inst + 2; //disp16, skip 2 bytes
        else
            inst = inst + 4; //disp32, skip 4 bytes
        break;
    }

    if ( op_size == QUAD )
        op_size = LONG;

    for ( i = 0; i < op_size; i++ ) {
        val |= (*inst++ & 0xff) << (8 * i);
    }

    return val;
}

static inline unsigned long get_immediate_sign_ext(
    int ad_size, const unsigned char *inst, int op_size)
{
    unsigned long result = get_immediate(ad_size, inst, op_size);
    if ( op_size == BYTE )
        return (int8_t)result;
    if ( op_size == WORD )
        return (int16_t)result;
    return (int32_t)result;
}

static inline int get_index(const unsigned char *inst, unsigned char rex)
{
    int mod, reg, rm;
    int rex_r, rex_b;

    mod = (*inst >> 6) & 3;
    reg = (*inst >> 3) & 7;
    rm = *inst & 7;

    rex_r = (rex >> 2) & 1;
    rex_b = rex & 1;

    //Only one operand in the instruction is register
    if ( mod == 3 ) {
        return (rm + (rex_b << 3));
    } else {
        return (reg + (rex_r << 3));
    }
    return 0;
}

static void init_instruction(struct hvm_io_op *mmio_op)
{
    mmio_op->instr = 0;

    mmio_op->flags = 0;

    mmio_op->operand[0] = 0;
    mmio_op->operand[1] = 0;
    mmio_op->immediate = 0;
}

#define GET_OP_SIZE_FOR_BYTE(size_reg)      \
    do {                                    \
        if ( rex )                          \
            (size_reg) = BYTE_64;           \
        else                                \
            (size_reg) = BYTE;              \
    } while( 0 )

#define GET_OP_SIZE_FOR_NONEBYTE(op_size)   \
    do {                                    \
        if ( rex & 0x8 )                    \
            (op_size) = QUAD;               \
        else if ( (op_size) != WORD )       \
            (op_size) = LONG;               \
    } while( 0 )


/*
 * Decode mem,accumulator operands (as in <opcode> m8/m16/m32, al,ax,eax)
 */
static inline int mem_acc(unsigned char size, struct hvm_io_op *mmio)
{
    mmio->operand[0] = mk_operand(size, 0, 0, MEMORY);
    mmio->operand[1] = mk_operand(size, 0, 0, REGISTER);
    return DECODE_success;
}

/*
 * Decode accumulator,mem operands (as in <opcode> al,ax,eax, m8/m16/m32)
 */
static inline int acc_mem(unsigned char size, struct hvm_io_op *mmio)
{
    mmio->operand[0] = mk_operand(size, 0, 0, REGISTER);
    mmio->operand[1] = mk_operand(size, 0, 0, MEMORY);
    return DECODE_success;
}

/*
 * Decode mem,reg operands (as in <opcode> r32/16, m32/16)
 */
static int mem_reg(unsigned char size, unsigned char *opcode,
                   struct hvm_io_op *mmio_op, unsigned char rex)
{
    int index = get_index(opcode + 1, rex);

    mmio_op->operand[0] = mk_operand(size, 0, 0, MEMORY);
    mmio_op->operand[1] = mk_operand(size, index, 0, REGISTER);
    return DECODE_success;
}

/*
 * Decode reg,mem operands (as in <opcode> m32/16, r32/16)
 */
static int reg_mem(unsigned char size, unsigned char *opcode,
                   struct hvm_io_op *mmio_op, unsigned char rex)
{
    int index = get_index(opcode + 1, rex);

    mmio_op->operand[0] = mk_operand(size, index, 0, REGISTER);
    mmio_op->operand[1] = mk_operand(size, 0, 0, MEMORY);
    return DECODE_success;
}


static inline unsigned char *check_prefix(unsigned char *inst,
                                          struct hvm_io_op *mmio_op,
                                          unsigned char *ad_size,
                                          unsigned char *op_size,
                                          unsigned char *seg_sel,
                                          unsigned char *rex_p)
{
    while ( 1 ) {
        switch ( *inst ) {
            /* rex prefix for em64t instructions */
        case 0x40 ... 0x4f:
            *rex_p = *inst;
            break;
        case 0xf3: /* REPZ */
            mmio_op->flags = REPZ;
            break;
        case 0xf2: /* REPNZ */
            mmio_op->flags = REPNZ;
            break;
        case 0xf0: /* LOCK */
            break;
        case 0x2e: /* CS */
        case 0x36: /* SS */
        case 0x3e: /* DS */
        case 0x26: /* ES */
        case 0x64: /* FS */
        case 0x65: /* GS */
            *seg_sel = *inst;
            break;
        case 0x66: /* 32bit->16bit */
            *op_size = WORD;
            break;
        case 0x67:
            *ad_size = WORD;
            break;
        default:
            return inst;
        }
        inst++;
    }
}

static int vmx_guest_x86_mode()
{

    if ( get_cpu().cr0.real_mode() )
        return 0;
    if ( get_vcpu().main_info.mrs.gpr_item.regs.eflags & X86_EFLAGS_VM)
        return 1;

    //TODO 64 bit architectur long mode
    return 4; //default size
}

static int hvm_guest_x86_mode(void)
{
    //TODO svm support
    
    //we use vmx
    return vmx_guest_x86_mode();
}

static void mmio_operands(int type, unsigned long gpa,
                          struct hvm_io_op *mmio_op,
                          unsigned char op_size)
{
    unsigned long value = 0;
    int index, size_reg;
    uint8_t df;
    L4_IA32_GPRegs_t *regs = mmio_op->io_context;

    df = regs->eflags & X86_EFLAGS_DF ? 1 : 0;

    size_reg = operand_size(mmio_op->operand[0]);

    if ( mmio_op->operand[0] & REGISTER ) {            /* dest is memory */
        index = operand_index(mmio_op->operand[0]);
        value = get_reg_value(size_reg, index, 0, regs);
        qemu_dm.send_mmio_req(type, gpa, 1, op_size, value, IOREQ_WRITE, df, 0);
    } else if ( mmio_op->operand[0] & IMMEDIATE ) {    /* dest is memory */
        value = mmio_op->immediate;
        qemu_dm.send_mmio_req(type, gpa, 1, op_size, value, IOREQ_WRITE, df, 0);
    } else if ( mmio_op->operand[0] & MEMORY ) {       /* dest is register */
        /* send the request and wait for the value */
        if ( (mmio_op->instr == INSTR_MOVZX) ||
             (mmio_op->instr == INSTR_MOVSX) )
            qemu_dm.send_mmio_req(type, gpa, 1, size_reg, 0, IOREQ_READ, df, 0);
        else
            qemu_dm.send_mmio_req(type, gpa, 1, op_size, 0, IOREQ_READ, df, 0);
    } else {
        printf("%s: invalid dest mode.\n", __func__);
	DEBUGGER_ENTER("untested");
    }
}


static int mmio_decode(int address_bytes, unsigned char *opcode,
                       struct hvm_io_op *mmio_op,
                       unsigned char *ad_size, unsigned char *op_size,
                       unsigned char *seg_sel)
{
    unsigned char size_reg = 0;
    unsigned char rex = 0;
    int index;

    *ad_size = 0;
    *op_size = 0;
    *seg_sel = 0;
    init_instruction(mmio_op);

    opcode = check_prefix(opcode, mmio_op, ad_size, op_size, seg_sel, &rex);

    switch ( address_bytes )
    {
    case 2:
        if ( *op_size == WORD )
            *op_size = LONG;
        else if ( *op_size == LONG )
            *op_size = WORD;
        else if ( *op_size == 0 )
            *op_size = WORD;
        if ( *ad_size == WORD )
            *ad_size = LONG;
        else if ( *ad_size == LONG )
            *ad_size = WORD;
        else if ( *ad_size == 0 )
            *ad_size = WORD;
        break;
    case 4:
        if ( *op_size == 0 )
            *op_size = LONG;
        if ( *ad_size == 0 )
            *ad_size = LONG;
        break;
#ifdef __x86_64__
    case 8:
        if ( *op_size == 0 )
            *op_size = rex & 0x8 ? QUAD : LONG;
        if ( *ad_size == WORD )
            *ad_size = LONG;
        else if ( *ad_size == 0 )
            *ad_size = QUAD;
        break;
#endif
    }

    /* the operands order in comments conforms to AT&T convention */

    switch ( *opcode ) {

    case 0x00: /* add r8, m8 */
        mmio_op->instr = INSTR_ADD;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x03: /* add m32/16, r32/16 */
        mmio_op->instr = INSTR_ADD;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_reg(*op_size, opcode, mmio_op, rex);

    case 0x08: /* or r8, m8 */	
        mmio_op->instr = INSTR_OR;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x09: /* or r32/16, m32/16 */
        mmio_op->instr = INSTR_OR;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x0A: /* or m8, r8 */
        mmio_op->instr = INSTR_OR;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return mem_reg(size_reg, opcode, mmio_op, rex);

    case 0x0B: /* or m32/16, r32/16 */
        mmio_op->instr = INSTR_OR;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_reg(*op_size, opcode, mmio_op, rex);

    case 0x20: /* and r8, m8 */
        mmio_op->instr = INSTR_AND;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x21: /* and r32/16, m32/16 */
        mmio_op->instr = INSTR_AND;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x22: /* and m8, r8 */
        mmio_op->instr = INSTR_AND;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return mem_reg(size_reg, opcode, mmio_op, rex);

    case 0x23: /* and m32/16, r32/16 */
        mmio_op->instr = INSTR_AND;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_reg(*op_size, opcode, mmio_op, rex);

    case 0x2B: /* sub m32/16, r32/16 */
        mmio_op->instr = INSTR_SUB;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_reg(*op_size, opcode, mmio_op, rex);

    case 0x30: /* xor r8, m8 */
        mmio_op->instr = INSTR_XOR;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x31: /* xor r32/16, m32/16 */
        mmio_op->instr = INSTR_XOR;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x32: /* xor m8, r8 */
        mmio_op->instr = INSTR_XOR;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return mem_reg(size_reg, opcode, mmio_op, rex);

    case 0x38: /* cmp r8, m8 */
        mmio_op->instr = INSTR_CMP;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x39: /* cmp r32/16, m32/16 */
        mmio_op->instr = INSTR_CMP;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x3A: /* cmp m8, r8 */
        mmio_op->instr = INSTR_CMP;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return mem_reg(size_reg, opcode, mmio_op, rex);

    case 0x3B: /* cmp m32/16, r32/16 */
        mmio_op->instr = INSTR_CMP;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_reg(*op_size, opcode, mmio_op, rex);

    case 0x80:
    case 0x81:
    case 0x83:
    {
        unsigned char ins_subtype = (opcode[1] >> 3) & 7;

        if ( opcode[0] == 0x80 ) {
            *op_size = BYTE;
            GET_OP_SIZE_FOR_BYTE(size_reg);
        } else {
            GET_OP_SIZE_FOR_NONEBYTE(*op_size);
            size_reg = *op_size;
        }

        /* opcode 0x83 always has a single byte operand */
        if ( opcode[0] == 0x83 )
            mmio_op->immediate =
                get_immediate_sign_ext(*ad_size, opcode + 1, BYTE);
        else
            mmio_op->immediate =
                get_immediate_sign_ext(*ad_size, opcode + 1, *op_size);

        mmio_op->operand[0] = mk_operand(size_reg, 0, 0, IMMEDIATE);
        mmio_op->operand[1] = mk_operand(size_reg, 0, 0, MEMORY);

        switch ( ins_subtype ) {
        case 0: /* add $imm, m32/16 */
            mmio_op->instr = INSTR_ADD;
            return DECODE_success;

        case 1: /* or $imm, m32/16 */
            mmio_op->instr = INSTR_OR;
            return DECODE_success;

        case 4: /* and $imm, m32/16 */
            mmio_op->instr = INSTR_AND;
            return DECODE_success;

        case 5: /* sub $imm, m32/16 */
            mmio_op->instr = INSTR_SUB;
            return DECODE_success;

        case 6: /* xor $imm, m32/16 */
            mmio_op->instr = INSTR_XOR;
            return DECODE_success;

        case 7: /* cmp $imm, m32/16 */
            mmio_op->instr = INSTR_CMP;
            return DECODE_success;

        default:
            dprintf(debug_qemu,"%x/%x, This opcode isn't handled yet!\n",
                   *opcode, ins_subtype);
            return DECODE_failure;
        }
    }

    case 0x84:  /* test r8, m8 */
        mmio_op->instr = INSTR_TEST;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x85: /* test r16/32, m16/32 */
        mmio_op->instr = INSTR_TEST;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x86:  /* xchg m8, r8 */
        mmio_op->instr = INSTR_XCHG;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x87:  /* xchg m16/32, r16/32 */
        mmio_op->instr = INSTR_XCHG;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x88: /* mov r8, m8 */
        mmio_op->instr = INSTR_MOV;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return reg_mem(size_reg, opcode, mmio_op, rex);

    case 0x89: /* mov r32/16, m32/16 */
        mmio_op->instr = INSTR_MOV;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return reg_mem(*op_size, opcode, mmio_op, rex);

    case 0x8A: /* mov m8, r8 */
        mmio_op->instr = INSTR_MOV;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return mem_reg(size_reg, opcode, mmio_op, rex);

    case 0x8B: /* mov m32/16, r32/16 */
        mmio_op->instr = INSTR_MOV;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_reg(*op_size, opcode, mmio_op, rex);

    case 0xA0: /* mov <addr>, al */
        mmio_op->instr = INSTR_MOV;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return mem_acc(size_reg, mmio_op);

    case 0xA1: /* mov <addr>, ax/eax */
        mmio_op->instr = INSTR_MOV;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return mem_acc(*op_size, mmio_op);

    case 0xA2: /* mov al, <addr> */
        mmio_op->instr = INSTR_MOV;
        *op_size = BYTE;
        GET_OP_SIZE_FOR_BYTE(size_reg);
        return acc_mem(size_reg, mmio_op);

    case 0xA3: /* mov ax/eax, <addr> */
        mmio_op->instr = INSTR_MOV;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return acc_mem(*op_size, mmio_op);

    case 0xA4: /* movsb */
        mmio_op->instr = INSTR_MOVS;
        *op_size = BYTE;
        return DECODE_success;

    case 0xA5: /* movsw/movsl */
        mmio_op->instr = INSTR_MOVS;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return DECODE_success;

    case 0xAA: /* stosb */
        mmio_op->instr = INSTR_STOS;
        *op_size = BYTE;
        return DECODE_success;

    case 0xAB: /* stosw/stosl */
        mmio_op->instr = INSTR_STOS;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return DECODE_success;

    case 0xAC: /* lodsb */
        mmio_op->instr = INSTR_LODS;
        *op_size = BYTE;
        return DECODE_success;

    case 0xAD: /* lodsw/lodsl */
        mmio_op->instr = INSTR_LODS;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        return DECODE_success;

    case 0xC6:
        if ( ((opcode[1] >> 3) & 7) == 0 ) { /* mov $imm8, m8 */
            mmio_op->instr = INSTR_MOV;
            *op_size = BYTE;

            mmio_op->operand[0] = mk_operand(*op_size, 0, 0, IMMEDIATE);
            mmio_op->immediate  =
                    get_immediate(*ad_size, opcode + 1, *op_size);
            mmio_op->operand[1] = mk_operand(*op_size, 0, 0, MEMORY);

            return DECODE_success;
        } else
            return DECODE_failure;

    case 0xC7:
        if ( ((opcode[1] >> 3) & 7) == 0 ) { /* mov $imm16/32, m16/32 */
            mmio_op->instr = INSTR_MOV;
            GET_OP_SIZE_FOR_NONEBYTE(*op_size);

            mmio_op->operand[0] = mk_operand(*op_size, 0, 0, IMMEDIATE);
            mmio_op->immediate =
                    get_immediate_sign_ext(*ad_size, opcode + 1, *op_size);
            mmio_op->operand[1] = mk_operand(*op_size, 0, 0, MEMORY);

            return DECODE_success;
        } else
            return DECODE_failure;

    case 0xF6:
    case 0xF7:
        if ( ((opcode[1] >> 3) & 7) == 0 ) { /* test $imm8/16/32, m8/16/32 */
            mmio_op->instr = INSTR_TEST;

            if ( opcode[0] == 0xF6 ) {
                *op_size = BYTE;
                GET_OP_SIZE_FOR_BYTE(size_reg);
            } else {
                GET_OP_SIZE_FOR_NONEBYTE(*op_size);
                size_reg = *op_size;
            }

            mmio_op->operand[0] = mk_operand(size_reg, 0, 0, IMMEDIATE);
            mmio_op->immediate =
                    get_immediate_sign_ext(*ad_size, opcode + 1, *op_size);
            mmio_op->operand[1] = mk_operand(size_reg, 0, 0, MEMORY);

            return DECODE_success;
        } else
            return DECODE_failure;

    case 0xFE:
    case 0xFF:
    {
        unsigned char ins_subtype = (opcode[1] >> 3) & 7;

        if ( opcode[0] == 0xFE ) {
            *op_size = BYTE;
            GET_OP_SIZE_FOR_BYTE(size_reg);
        } else {
            GET_OP_SIZE_FOR_NONEBYTE(*op_size);
            size_reg = *op_size;
        }

        mmio_op->immediate = 1;
        mmio_op->operand[0] = mk_operand(size_reg, 0, 0, IMMEDIATE);
        mmio_op->operand[1] = mk_operand(size_reg, 0, 0, MEMORY);

        switch ( ins_subtype ) {
        case 0: /* inc */
            mmio_op->instr = INSTR_ADD;
            return DECODE_success;

        case 1: /* dec */
            mmio_op->instr = INSTR_SUB;
            return DECODE_success;

        case 6: /* push */
            mmio_op->instr = INSTR_PUSH;
            mmio_op->operand[0] = mmio_op->operand[1];
            return DECODE_success;

        default:
            dprintf(debug_qemu,"%x/%x, This opcode isn't handled yet!\n",
                   *opcode, ins_subtype);
            return DECODE_failure;
        }
    }

    case 0x0F:
        break;

    default:
        dprintf(debug_qemu,"%x, This opcode isn't handled yet!\n", *opcode);
        return DECODE_failure;
    }

    switch ( *++opcode ) {
    case 0xB6: /* movzx m8, r16/r32/r64 */
        mmio_op->instr = INSTR_MOVZX;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        index = get_index(opcode + 1, rex);
        mmio_op->operand[0] = mk_operand(BYTE, 0, 0, MEMORY);
        mmio_op->operand[1] = mk_operand(*op_size, index, 0, REGISTER);
        return DECODE_success;

    case 0xB7: /* movzx m16, r32/r64 */
        mmio_op->instr = INSTR_MOVZX;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        index = get_index(opcode + 1, rex);
        mmio_op->operand[0] = mk_operand(WORD, 0, 0, MEMORY);
        mmio_op->operand[1] = mk_operand(*op_size, index, 0, REGISTER);
        return DECODE_success;

    case 0xBE: /* movsx m8, r16/r32/r64 */
        mmio_op->instr = INSTR_MOVSX;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        index = get_index(opcode + 1, rex);
        mmio_op->operand[0] = mk_operand(BYTE, 0, 0, MEMORY);
        mmio_op->operand[1] = mk_operand(*op_size, index, 0, REGISTER);
        return DECODE_success;

    case 0xBF: /* movsx m16, r32/r64 */
        mmio_op->instr = INSTR_MOVSX;
        GET_OP_SIZE_FOR_NONEBYTE(*op_size);
        index = get_index(opcode + 1, rex);
        mmio_op->operand[0] = mk_operand(WORD, 0, 0, MEMORY);
        mmio_op->operand[1] = mk_operand(*op_size, index, 0, REGISTER);
        return DECODE_success;

    case 0xA3: /* bt r32, m32 */
        mmio_op->instr = INSTR_BT;
        index = get_index(opcode + 1, rex);
        *op_size = LONG;
        mmio_op->operand[0] = mk_operand(*op_size, index, 0, REGISTER);
        mmio_op->operand[1] = mk_operand(*op_size, 0, 0, MEMORY);
        return DECODE_success;

    case 0xBA:
        if ( ((opcode[1] >> 3) & 7) == 4 ) /* BT $imm8, m16/32/64 */
        {
            mmio_op->instr = INSTR_BT;
            GET_OP_SIZE_FOR_NONEBYTE(*op_size);
            mmio_op->operand[0] = mk_operand(BYTE, 0, 0, IMMEDIATE);
            mmio_op->immediate =
                    (signed char)get_immediate(*ad_size, opcode + 1, BYTE);
            mmio_op->operand[1] = mk_operand(*op_size, 0, 0, MEMORY);
            return DECODE_success;
        }
        else
        {
            dprintf(debug_qemu,"0f %x, This opcode subtype isn't handled yet\n", *opcode);
            return DECODE_failure;
        }

    default:
        dprintf(debug_qemu,"0f %x, This opcode isn't handled yet\n", *opcode);
        return DECODE_failure;
    }
}

#define GET_REPEAT_COUNT() \
     (mmio_op->flags & REPZ ? (ad_size == WORD ? regs->ecx & 0xFFFF : regs->ecx) : 1)

void handle_mmio(word_t gpa)
{
    mrs_t *vcpu_mrs = &get_vcpu().main_info.mrs;

    word_t inst_addr;
    struct hvm_io_op *mmio_op;
    L4_IA32_GPRegs_t *regs;
    uint8_t inst[MAX_INST_LEN], ad_size, op_size, seg_sel;
    int i, address_bytes, inst_len;
    uint8_t df;

    regs = &vcpu_mrs->gpr_item.regs;
    mmio_op = &qemu_dm.mmio_op;
    mmio_op->io_context = regs;

    //store seg values
    word_t mask = 0xff;
    if(!backend_async_read_segregs(mask))
    {
	printf("updating segmentregister failed \n");
	DEBUGGER_ENTER("Untested");
    }

    df = regs->eflags & X86_EFLAGS_DF ? 1 : 0;

    address_bytes = hvm_guest_x86_mode();
    if (address_bytes < 2)
    {
	/* real or vm86 modes */
        address_bytes = 2;
	if(!backend_async_read_eaddr(L4_CTRLXFER_CSREGS_ID, vcpu_mrs->gpr_item.regs.eip, inst_addr))
	{
	    dprintf(debug_qemu,"mmio: backend_async_read in vm86 mode failed\n");
	}
	
    }
    else if(address_bytes == 4)
    {
	word_t psize;
	backend_resolve_kaddr(vcpu_mrs->gpr_item.regs.eip,4,inst_addr,psize);
	if(psize != 4)
	    dprintf(debug_qemu,"mmio: backend resolve returned with invalid size %u\n",psize);
    }

    memset(inst, 0, MAX_INST_LEN);
    inst_len = hvm_instruction_fetch(inst_addr, address_bytes, inst);
    if ( inst_len <= 0 )
    {
	dprintf(debug_qemu, "handle_mmio: failed to get instruction\n");
        /* hvm_instruction_fetch() will have injected a #PF; get out now */
        return;
    }

    if ( mmio_decode(address_bytes, inst, mmio_op, &ad_size,
                     &op_size, &seg_sel) == DECODE_failure )
    {
        printf("handle_mmio: failed to decode instruction\n");
        printf("mmio opcode: gpa 0x%lx, len %d:", gpa, inst_len);
        for ( i = 0; i < inst_len; i++ )
            printf(" %02x", inst[i] & 0xFF);
        printf("\n");

	DEBUGGER_ENTER("untested");
        return;
    }

    regs->eip += inst_len; /* advance %eip */

    switch ( mmio_op->instr ) {
    case INSTR_MOV:
        mmio_operands(IOREQ_TYPE_COPY, gpa, mmio_op, op_size);
        break;

    case INSTR_MOVS:
    {
        unsigned long count = GET_REPEAT_COUNT();
        int sign = regs->eflags & X86_EFLAGS_DF ? -1 : 1;
        unsigned long addr;
        word_t paddr = 0;
        int dir, size = op_size;

        ASSERT(count);

        /* determine non-MMIO address */
        addr = regs->edi;
        if ( ad_size == WORD )
            addr &= 0xFFFF;
	resolve_operand_addr(x86_seg_es,addr,paddr,address_bytes);
        if ( paddr == gpa )
        {
            enum x86_segment seg;
	    seg = x86_seg_none;//avoid warning

            dir = IOREQ_WRITE;
            addr = regs->esi;
            if ( ad_size == WORD )
                addr &= 0xFFFF;
            switch ( seg_sel )
            {
            case 0x26: seg = x86_seg_es; break;
            case 0x2e: seg = x86_seg_cs; break;
            case 0x36: seg = x86_seg_ss; break;
            case 0:
            case 0x3e: seg = x86_seg_ds; break;
            case 0x64: seg = x86_seg_fs; break;
            case 0x65: seg = x86_seg_gs; break;
		printf("seg_sel isn't valid segment\n");
	    default:DEBUGGER_ENTER("untested");	
            }
	    resolve_operand_addr(seg,addr,paddr,address_bytes);
        }
        else
            dir = IOREQ_READ;

        /*
         * In case of a movs spanning multiple pages, we break the accesses
         * up into multiple pages (the device model works with non-continguous
         * physical guest pages). To copy just one page, we adjust %ecx and
         * do not advance %eip so that the next rep;movs copies the next page.
         * Unaligned accesses, for example movsl starting at PGSZ-2, are
         * turned into a single copy where we handle the overlapping memory
         * copy ourself. After this copy succeeds, "rep movs" is executed
         * again.
         */
        if ( (addr & PAGE_MASK) != ((addr + size - 1) & PAGE_MASK) ) {
            unsigned long value = 0;

            mmio_op->flags |= OVERLAP;

            if ( dir == IOREQ_WRITE ) {
		dprintf(debug_qemu,"call copy_from_guest_phyx addr %lx siz %d\n",addr,size);
		(void)hvm_copy_from_guest_phys(&value, addr, size);
            } else /* dir != IOREQ_WRITE */
                /* Remember where to write the result, as a *VA*.
                 * Must be a VA so we can handle the page overlap 
                 * correctly in hvm_mmio_assist() */
                mmio_op->addr = addr;

            if ( count != 1 )
                regs->eip -= inst_len; /* do not advance %eip */

            qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa, 1, size, value, dir, df, 0);
        } else {
            unsigned long last_addr = sign > 0 ? addr + count * size - 1
                                               : addr - (count - 1) * size;

            if ( (addr & PAGE_MASK) != (last_addr & PAGE_MASK) )
            {
                regs->eip -= inst_len; /* do not advance %eip */

                if ( sign > 0 )
                    count = (PAGE_SIZE - (addr & ~PAGE_MASK)) / size;
                else
                    count = (addr & ~PAGE_MASK) / size + 1;
            }

            ASSERT(count);

            qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa, count, size, 
                          paddr, dir, df, 1);
        }
        break;
    }

    case INSTR_MOVZX:
    case INSTR_MOVSX:
        mmio_operands(IOREQ_TYPE_COPY, gpa, mmio_op, op_size);
        break;

    case INSTR_STOS:
        /*
         * Since the destination is always in (contiguous) mmio space we don't
         * need to break it up into pages.
         */
        qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa,
                      GET_REPEAT_COUNT(), op_size, regs->eax, IOREQ_WRITE, df, 0);
        break;

    case INSTR_LODS:
        /*
         * Since the source is always in (contiguous) mmio space we don't
         * need to break it up into pages.
         */
        mmio_op->operand[0] = mk_operand(op_size, 0, 0, REGISTER);
	printf("count 3 = %d\n",GET_REPEAT_COUNT());
        qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa,
                      GET_REPEAT_COUNT(), op_size, 0, IOREQ_READ, df, 0);
        break;

    case INSTR_OR:
        mmio_operands(IOREQ_TYPE_OR, gpa, mmio_op, op_size);
        break;

    case INSTR_AND:
        mmio_operands(IOREQ_TYPE_AND, gpa, mmio_op, op_size);
        break;

    case INSTR_ADD:
        mmio_operands(IOREQ_TYPE_ADD, gpa, mmio_op, op_size);
        break;

    case INSTR_SUB:
        mmio_operands(IOREQ_TYPE_SUB, gpa, mmio_op, op_size);
        break;

    case INSTR_XOR:
        mmio_operands(IOREQ_TYPE_XOR, gpa, mmio_op, op_size);
        break;

    case INSTR_PUSH:
        if ( ad_size == WORD )
        {
            mmio_op->addr = (uint16_t)(regs->esp - op_size);
            regs->esp = mmio_op->addr | (regs->esp & ~0xffff);
        }
        else
        {
            regs->esp -= op_size;
            mmio_op->addr = regs->esp;
        }
        /* send the request and wait for the value */
        qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa, 1, op_size, 0, IOREQ_READ, df, 0);
        break;

    case INSTR_CMP:        /* Pass through */
    case INSTR_TEST:
        /* send the request and wait for the value */
        qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa, 1, op_size, 0, IOREQ_READ, df, 0);
        break;

    case INSTR_BT:
    {
        unsigned long value = 0;
        int index, size;

        if ( mmio_op->operand[0] & REGISTER )
        {
            index = operand_index(mmio_op->operand[0]);
            size = operand_size(mmio_op->operand[0]);
            value = get_reg_value(size, index, 0, regs);
        }
        else if ( mmio_op->operand[0] & IMMEDIATE )
        {
            mmio_op->immediate = mmio_op->immediate;
            value = mmio_op->immediate;
        }
        qemu_dm.send_mmio_req(IOREQ_TYPE_COPY, gpa + (value >> 5), 1,
                      op_size, 0, IOREQ_READ, df, 0);
        break;
    }

    case INSTR_XCHG:
        if ( mmio_op->operand[0] & REGISTER ) {
            long value;
            unsigned long operand = mmio_op->operand[0];
            value = get_reg_value(operand_size(operand),
                                  operand_index(operand), 0,
                                  regs);
            /* send the request and wait for the value */
            qemu_dm.send_mmio_req(IOREQ_TYPE_XCHG, gpa, 1,
                          op_size, value, IOREQ_WRITE, df, 0);
        } else {
            /* the destination is a register */
            long value;
            unsigned long operand = mmio_op->operand[1];
            value = get_reg_value(operand_size(operand),
                                  operand_index(operand), 0,
                                  regs);
            /* send the request and wait for the value */
            qemu_dm.send_mmio_req(IOREQ_TYPE_XCHG, gpa, 1,
                          op_size, value, IOREQ_WRITE, df, 0);
        }
        break;

    default:
        printf("Unhandled MMIO instruction\n");
	DEBUGGER_ENTER("untested");	
    }
}


extern qemu_dm_t qemu_dm;
void hvm_mmio_assist(void)
{
    struct hvm_io_op *mmio_opp = &qemu_dm.mmio_op;
    L4_IA32_GPRegs_t *regs = mmio_opp->io_context;
    ioreq_t *p = &qemu_dm.s_pages.sio_page.vcpu_iodata[0].vp_ioreq;


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
        regs->eflags &= ~(X86_EFLAGS_CF|X86_EFLAGS_PF|
                          X86_EFLAGS_ZF|X86_EFLAGS_SF|X86_EFLAGS_OF);
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
        regs->eflags &= ~(X86_EFLAGS_CF|X86_EFLAGS_PF|X86_EFLAGS_AF|
                          X86_EFLAGS_ZF|X86_EFLAGS_SF|X86_EFLAGS_OF);
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
        regs->eflags &= ~(X86_EFLAGS_CF|X86_EFLAGS_PF|
                          X86_EFLAGS_ZF|X86_EFLAGS_SF|X86_EFLAGS_OF);
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
        regs->eflags &= ~(X86_EFLAGS_CF|X86_EFLAGS_PF|
                          X86_EFLAGS_ZF|X86_EFLAGS_SF|X86_EFLAGS_OF);
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
        regs->eflags &= ~(X86_EFLAGS_CF|X86_EFLAGS_PF|X86_EFLAGS_AF|
                          X86_EFLAGS_ZF|X86_EFLAGS_SF|X86_EFLAGS_OF);
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
        regs->eflags &= ~(X86_EFLAGS_CF|X86_EFLAGS_PF|
                          X86_EFLAGS_ZF|X86_EFLAGS_SF|X86_EFLAGS_OF);
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
            regs->eflags |= X86_EFLAGS_CF;
        else
            regs->eflags &= ~X86_EFLAGS_CF;

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

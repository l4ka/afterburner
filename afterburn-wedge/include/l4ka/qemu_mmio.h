#include INC_ARCH(types.h)
#include <debug.h>
#include INC_WEDGE(l4thread.h)
#include <console.h>
#include <string.h>

#include <l4/ia32/arch.h>
#include INC_WEDGE(qemu_dm.h)

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

#define MAX_IO_HANDLER             12

#define HVM_PORTIO                  0
#define HVM_MMIO                    1
#define HVM_BUFFERED_IO             2


#define X86_EFLAGS_CF	0x00000001 /* Carry Flag */
#define X86_EFLAGS_PF	0x00000004 /* Parity Flag */
#define X86_EFLAGS_AF	0x00000010 /* Auxillary carry Flag */
#define X86_EFLAGS_ZF	0x00000040 /* Zero Flag */
#define X86_EFLAGS_SF	0x00000080 /* Sign Flag */
#define X86_EFLAGS_TF	0x00000100 /* Trap Flag */
#define X86_EFLAGS_IF	0x00000200 /* Interrupt Flag */
#define X86_EFLAGS_DF	0x00000400 /* Direction Flag */
#define X86_EFLAGS_OF	0x00000800 /* Overflow Flag */
#define X86_EFLAGS_IOPL	0x00003000 /* IOPL mask */
#define X86_EFLAGS_NT	0x00004000 /* Nested Task */
#define X86_EFLAGS_RF	0x00010000 /* Resume Flag */
#define X86_EFLAGS_VM	0x00020000 /* Virtual Mode */
#define X86_EFLAGS_AC	0x00040000 /* Alignment Check */
#define X86_EFLAGS_VIF	0x00080000 /* Virtual Interrupt Flag */
#define X86_EFLAGS_VIP	0x00100000 /* Virtual Interrupt Pending */
#define X86_EFLAGS_ID	0x00200000 /* CPUID detection flag */

/* l4ka/qemu_mmio.cc */
void handle_mmio(word_t gpa); 
void hvm_mmio_assist(void);
void hvm_copy_from_guest_phys(void *buf, word_t addr, word_t size);
word_t qemu_is_mmio_area(word_t addr);

/* l4ka/qemu_instlen.c */
int hvm_instruction_fetch(unsigned long org_pc, int address_bytes,
                          unsigned char *buf);

/* 
 * Attribute for segment selector. This is a copy of bit 40:47 & 52:55 of the
 * segment descriptor. It happens to match the format of an AMD SVM VMCB.
 */
typedef union segment_attributes {
    uint16_t bytes;
    struct
    {
        uint16_t type:4;    /* 0;  Bit 40-43 */
        uint16_t s:   1;    /* 4;  Bit 44 */
        uint16_t dpl: 2;    /* 5;  Bit 45-46 */
        uint16_t p:   1;    /* 7;  Bit 47 */
        uint16_t avl: 1;    /* 8;  Bit 52 */
        uint16_t l:   1;    /* 9;  Bit 53 */
        uint16_t db:  1;    /* 10; Bit 54 */
        uint16_t g:   1;    /* 11; Bit 55 */
    } fields;
} __attribute__ ((packed)) segment_attributes_t;

/*
 * Full state of a segment register (visible and hidden portions).
 * Again, this happens to match the format of an AMD SVM VMCB.
 */
struct segment_register {
    uint16_t        sel;
    segment_attributes_t attr;
    uint32_t        limit;
    uint64_t        base;
} __attribute__ ((packed));

/* Comprehensive enumeration of x86 segment registers. */
enum x86_segment {
    /* General purpose. */
    x86_seg_cs = L4_CTRLXFER_CSREGS_ID,
    x86_seg_ss = L4_CTRLXFER_SSREGS_ID,
    x86_seg_ds = L4_CTRLXFER_DSREGS_ID,
    x86_seg_es = L4_CTRLXFER_ESREGS_ID,
    x86_seg_fs = L4_CTRLXFER_FSREGS_ID,
    x86_seg_gs = L4_CTRLXFER_GSREGS_ID,
    /* System. */
    x86_seg_tr = L4_CTRLXFER_TRREGS_ID,
    x86_seg_ldtr = L4_CTRLXFER_LDTRREGS_ID,
    x86_seg_gdtr = L4_CTRLXFER_GDTRREGS_ID,
    x86_seg_idtr = L4_CTRLXFER_IDTRREGS_ID,
    /*
     * Dummy: used to emulate direct processor accesses to management
     * structures (TSS, GDT, LDT, IDT, etc.) which use linear addressing
     * (no segment component) and bypass usual segment- and page-level
     * protection checks.
     */
    x86_seg_none = L4_CTRLXFER_INVALID_ID,
};

/* #PF error code values. */
#define PFEC_page_present   (1U<<0)
#define PFEC_write_access   (1U<<1)
#define PFEC_user_mode      (1U<<2)
#define PFEC_reserved_bit   (1U<<3)
#define PFEC_insn_fetch     (1U<<4)

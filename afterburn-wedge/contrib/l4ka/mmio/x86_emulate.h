/******************************************************************************
 * x86_emulate.h
 * 
 * Generic x86 (32-bit and 64-bit) instruction decoder and emulator.
 * 
 * Copyright (c) 2005-2007 Keir Fraser
 * Copyright (c) 2005-2007 XenSource Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __X86_EMULATE_H__
#define __X86_EMULATE_H__

struct x86_emulate_ctxt;

/* Comprehensive enumeration of x86 segment registers. */
enum x86_segment {
    /* General purpose. */
    x86_seg_cs,
    x86_seg_ss,
    x86_seg_ds,
    x86_seg_es,
    x86_seg_fs,
    x86_seg_gs,
    /* System. */
    x86_seg_tr,
    x86_seg_ldtr,
    x86_seg_gdtr,
    x86_seg_idtr,
    /*
     * Dummy: used to emulate direct processor accesses to management
     * structures (TSS, GDT, LDT, IDT, etc.) which use linear addressing
     * (no segment component) and bypass usual segment- and page-level
     * protection checks.
     */
    x86_seg_none
};

#define is_x86_user_segment(seg) ((unsigned)(seg) <= x86_seg_gs)

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


#endif /* __X86_EMULATE_H__ */

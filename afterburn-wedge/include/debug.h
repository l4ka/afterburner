/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/debug.h
 * Description:   Debug support.
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
 * $Id: debug.h,v 1.7 2006/03/29 14:14:38 joshua Exp $
 *
 ********************************************************************/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#define DBG_LEVEL	3
#define TRACE_LEVEL	6

#include INC_ARCH(types.h)
#include INC_WEDGE(debug.h)



const word_t debug_startup=0;
const word_t debug_idle=3;

const word_t debug_preemption=3;
const word_t debug_exception=3;
const word_t debug_pfault=3;
const word_t debug_iret=3;

const word_t debug_superpages=3;
const word_t debug_page_not_present=0;
const word_t debug_unmap = 3;
const word_t debug_syscall=3;

const word_t debug_user_access=3;
const word_t debug_user_signal=3;

const word_t debug_task = 3;

/******** rewriter debugging **************/
const word_t debug_reloc = 3;
const word_t debug_resolve = 3;
const word_t debug_nop_space = 3;
const word_t debug_elf = 0;


/******** vCPU debugging **************/
const word_t debug_dtr=3;

const word_t debug_cr0_write=3;
const word_t debug_cr2_write=3;
const word_t debug_cr3_write=3;
const word_t debug_cr4_write=3;
const word_t debug_cr_read=3;

const word_t debug_seg_write=3;
const word_t debug_seg_read=3;
const word_t debug_movseg=3;

const word_t debug_ltr=3;
const word_t debug_str=3;

const word_t debug_dr=3;

const word_t debug_port_io=3;
const word_t debug_port_io_unhandled=0;


const word_t debug_flush=3;
const word_t debug_msr=3;


/******** Device and IRQ debugging **************/
const word_t debug_device =3;
const word_t debug_dma    =3;

const word_t debug_acpi=3;
const word_t debug_apic=3;
const bool debug_apic_sanity=true;


const  word_t debug_irq    =3;
extern word_t irq_traced;
extern word_t vector_traced[8];

INLINE void dbg_irq(word_t irq)
{ irq_traced |= (1 << irq); } 
INLINE void undbg_irq(word_t irq)
{ irq_traced &= ~(1 << irq); } 
INLINE void dbg_vector(word_t vector)
{  vector_traced[vector >> 5] |= (1<< (vector & 0x1f)); }
INLINE void undbg_vector(word_t vector)
{  vector_traced[vector >> 5] &= ~(1<< (vector & 0x1f)); }

INLINE word_t irq_dbg_level(word_t irq, word_t vector = 0)
{
    word_t level;
    if ((irq < 256) && (irq_traced & (1<<irq)))
	level = 0;
    else if (vector > 0 && vector_traced[vector >> 5] & (1<< (vector & 0x1f)))
	level = 0;
    else 
	level = debug_irq;
    
    return level;
} 



#endif /* !__DEBUG_H__ */

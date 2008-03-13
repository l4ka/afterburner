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

#ifdef CONFIG_WEDGE_KAXEN
#define DEBUGGER_ENTER_M(a) PANIC(a)
#else
#define DEBUGGER_ENTER_M(a) DEBUGGER_ENTER(a)
#endif

#define DBG_LEVEL	3
#define TRACE_LEVEL	6

#include INC_ARCH(types.h)
#include INC_WEDGE(debug.h)

#define DEBUG_STATIC __attribute__((unused)) static  

DEBUG_STATIC debug_id_t debug_lock		= debug_id_t( 0, 4);

DEBUG_STATIC debug_id_t debug_startup		= debug_id_t( 1, 0);
DEBUG_STATIC debug_id_t debug_idle		= debug_id_t( 2, 3);

DEBUG_STATIC debug_id_t debug_preemption	= debug_id_t( 3, 3);
DEBUG_STATIC debug_id_t debug_exception		= debug_id_t( 4, 3);
DEBUG_STATIC debug_id_t debug_pfault		= debug_id_t( 5, 3);
DEBUG_STATIC debug_id_t debug_iret		= debug_id_t( 6, 3);

DEBUG_STATIC debug_id_t debug_superpages	= debug_id_t( 7, 3);
DEBUG_STATIC debug_id_t debug_page_not_present	= debug_id_t( 8, 0);
DEBUG_STATIC debug_id_t debug_unmap		= debug_id_t( 9, 3);
DEBUG_STATIC debug_id_t debug_syscall		= debug_id_t(10, 3);

DEBUG_STATIC debug_id_t debug_user_access	= debug_id_t(11, 3);
DEBUG_STATIC debug_id_t debug_user_signal	= debug_id_t(12, 3);

DEBUG_STATIC debug_id_t debug_task 		= debug_id_t(13, 3);

/******** rewriter debugging **************/
DEBUG_STATIC debug_id_t debug_reloc 		= debug_id_t(14, 3);
DEBUG_STATIC debug_id_t debug_resolve 		= debug_id_t(15, 3);
DEBUG_STATIC debug_id_t debug_patchup 	        = debug_id_t(16, 3);
DEBUG_STATIC debug_id_t debug_elf		= debug_id_t(17, 0);


/******** vCPU debugging **************/
DEBUG_STATIC debug_id_t debug_dtr		= debug_id_t(18, 3);

DEBUG_STATIC debug_id_t debug_cr0_write		= debug_id_t(19, 0);
DEBUG_STATIC debug_id_t debug_cr2_write		= debug_id_t(20, 0);
DEBUG_STATIC debug_id_t debug_cr3_write		= debug_id_t(21, 0);
DEBUG_STATIC debug_id_t debug_cr4_write		= debug_id_t(22, 0);
DEBUG_STATIC debug_id_t debug_cr_read		= debug_id_t(23, 0);

DEBUG_STATIC debug_id_t debug_seg_write		= debug_id_t(24, 3);
DEBUG_STATIC debug_id_t debug_seg_read		= debug_id_t(25, 3);
DEBUG_STATIC debug_id_t debug_movseg		= debug_id_t(26, 3);

DEBUG_STATIC debug_id_t debug_ltr		= debug_id_t(27, 3);
DEBUG_STATIC debug_id_t debug_str		= debug_id_t(28, 3);

DEBUG_STATIC debug_id_t debug_dr		= debug_id_t(29, 0);

DEBUG_STATIC debug_id_t debug_portio		= debug_id_t(30, 4);
DEBUG_STATIC debug_id_t debug_portio_unhandled	= debug_id_t(31, 3);


DEBUG_STATIC debug_id_t debug_flush		= debug_id_t(32, 3);
DEBUG_STATIC debug_id_t debug_msr		= debug_id_t(33, 3);

/******** Device and IRQ debugging **************/
DEBUG_STATIC debug_id_t debug_irq    		= debug_id_t(34, 3);
DEBUG_STATIC debug_id_t debug_device 		= debug_id_t(35, 3);
DEBUG_STATIC debug_id_t debug_dma    		= debug_id_t(36, 4);

DEBUG_STATIC debug_id_t debug_acpi		= debug_id_t(37, 3);
DEBUG_STATIC debug_id_t debug_apic		= debug_id_t(38, 4);
DEBUG_STATIC bool	debug_apic_sanity=true;

DEBUG_STATIC debug_id_t debug_ide		= debug_id_t(39, 3);
DEBUG_STATIC debug_id_t debug_ide_request	= debug_id_t(40, 3);
DEBUG_STATIC debug_id_t debug_ide_ddos		= debug_id_t(41, 3);
DEBUG_STATIC debug_id_t debug_ide_i82371	= debug_id_t(42, 3);

DEBUG_STATIC debug_id_t debug_dp83820_init	= debug_id_t(43, 0);
DEBUG_STATIC debug_id_t debug_dp83820_tx	= debug_id_t(44, 0);
DEBUG_STATIC debug_id_t debug_dp83820_rx	= debug_id_t(45, 0);

/******** HVM debugging **************/
DEBUG_STATIC debug_id_t debug_hvm_fault		= debug_id_t(46, 0); 
DEBUG_STATIC debug_id_t debug_hvm_io		= debug_id_t(47, 0);
DEBUG_STATIC debug_id_t debug_hvm_irq		= debug_id_t(48, 0);

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

INLINE debug_id_t irq_dbg_level(word_t irq, word_t vector = 0)
{
    debug_id_t id(debug_irq.id, debug_irq.level);
    
    if ((irq < 256) && (irq_traced & (1<<irq)))
	id.level = 0;
    else if (vector > 0 && vector_traced[vector >> 5] & (1<< (vector & 0x1f)))
	id.level = 0;
    
    return id;
} 



#endif /* !__DEBUG_H__ */

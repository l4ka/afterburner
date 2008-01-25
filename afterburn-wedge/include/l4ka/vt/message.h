/*********************************************************************
 *
 * Copyright (C) 2006,  Karlsruhe University
 *
 * File path:     testvirt/ia32.h
 * Description:
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
 * $Id:$
 *
 ********************************************************************/
#ifndef __L4KA__VT__IA32_H__
#define __L4KA__VT__IA32_H__
#include <ia32/types.h>
#include <ia32/bitops.h>
// #include INC_WEDGE(vt/portaccess.h)
#include <ia32/instr.h>
#include <ia32/page.h>

#define L4_IO_PAGEFAULT         (-8UL << 20)

#define INVALID_ADDR		(-1UL)

#define L4_LABEL_INTR			(0xfff)
#define L4_LABEL_PFAULT			(0xffe)
#define L4_LABEL_PREEMPT		(0xffd)
#define L4_LABEL_EXCEPT			(0xffc)
#define L4_LABEL_IO_PAGEFAULT		(0xff7)
#define L4_LABEL_VECTOR                 (0x102)


/*
 * There exist different kernel generated
 * virtualization fault messages.
 * They are identified by different IPC labels.
 */
#define L4_LABEL_REGISTER_FAULT		(0xff8)
#define L4_LABEL_INSTRUCTION_FAULT	(0xff7)
#define L4_LABEL_EXCEPTION_FAULT	(0xff6)
#define L4_LABEL_IO_FAULT		(0xff5)
#define L4_LABEL_MSR_FAULT		(0xff4)
#define L4_LABEL_DELAYED_FAULT		(0xff3)
#define L4_LABEL_IMMEDIATE_FAULT	(0xff2)
#define L4_LABEL_VIRT_NORESUME		(0xff1)
#define L4_LABEL_VIRT_ERROR		(0xff0)

#define L4_NUM_BASIC_EXIT_REASONS	(33)
#define L4_EXIT_REASON_OFFSET		(0x10)

/*
 * Only one reply format is used.
 */
#define L4_LABEL_VFAULT_REPLY		(0xff9)


#endif /* !__L4KA__VT__IA32_H__ */

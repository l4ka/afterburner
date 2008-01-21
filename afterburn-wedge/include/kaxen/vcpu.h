/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/kaxen/vcpu.h
 * Description:   The per-CPU data declarations for the L4Ka environment.
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
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__VCPU_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__VCPU_H__

#define OFS_CPU_FLAGS	0
#define OFS_CPU_CS	4
#define OFS_CPU_SS	8
#define OFS_CPU_TSS	12
#define OFS_CPU_IDTR	18
#define OFS_CPU_CR2	40
#define OFS_CPU_REDIRECT	64
#define OFS_CPU_ESP0	108


#if defined(ASSEMBLY)

.macro VCPU reg
	lea	vcpu, \reg
.endm

#else

#include INC_ARCH(cpu.h)

// TODO amd64
struct vcpu_t
{
    cpu_t cpu;

    word_t guest_vaddr_offset;	// 104
    word_t xen_esp0;		// 108

    // TODO: use 64-bit type for cpu_hz
    word_t cpu_hz;
    word_t cpu_id;

    word_t get_id()
	{ return cpu_id; }

    void set_kernel_vaddr( word_t vaddr )
	{ guest_vaddr_offset = vaddr; }
    word_t get_kernel_vaddr()
	{ return guest_vaddr_offset; }

#if 0
    word_t get_wedge_vaddr()
	{ return CONFIG_WEDGE_VIRT; }
    word_t get_wedge_end_vaddr()
	{ return CONFIG_WEDGE_VIRT_END; }
    word_t get_wedge_paddr()
	{ return CONFIG_WEDGE_PHYS; }
    word_t get_window_start()
	{ return CONFIG_WEDGE_VIRT; }
    word_t get_window_end()
	{ return CONFIG_WEDGE_WINDOW + CONFIG_WEDGE_VIRT; }
#endif
};

#endif	/* ASSEMBLY */

#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__VCPU_H__ */

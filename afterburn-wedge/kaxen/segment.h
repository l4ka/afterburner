/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/segment.h
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
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__SEGMENT_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__SEGMENT_H__

#include INC_WEDGE(xen_asm.h)

#if defined(CONFIG_LINUX_HEURISTICS)

/* We know the segment descriptors of Linux, so hard code them.
 */

#if defined(CONFIG_LINUX_2_6)
# define GUEST_CS_BOOT_SEGMENT	2
# define GUEST_DS_BOOT_SEGMENT	3
# define GUEST_CS_KERNEL_SEGMENT	12
# define GUEST_DS_KERNEL_SEGMENT	13
# define GUEST_CS_USER_SEGMENT		14
# define GUEST_DS_USER_SEGMENT		15
#elif defined(CONFIG_LINUX_2_4)
# define GUEST_CS_KERNEL_SEGMENT	2
# define GUEST_DS_KERNEL_SEGMENT	3
# define GUEST_CS_BOOT_SEGMENT		2
# define GUEST_DS_BOOT_SEGMENT		3
# define GUEST_CS_USER_SEGMENT		4
# define GUEST_DS_USER_SEGMENT		5
#endif

#define XEN_CS_KERNEL		((GUEST_CS_KERNEL_SEGMENT << 3) | 1)
#define XEN_DS_KERNEL		((GUEST_DS_KERNEL_SEGMENT << 3) | 1)

#define XEN_CS_USER		((GUEST_CS_USER_SEGMENT << 3) | 3)
#define XEN_DS_USER		((GUEST_DS_USER_SEGMENT << 3) | 3)

#else

/* We don't know the segment descriptors of the guest, or perhaps we want to be
 * more versatile, so virtualize the segments.
 */

#define XEN_CS_KERNEL	FLAT_GUESTOS_CS
#define XEN_DS_KERNEL	FLAT_GUESTOS_DS
#define XEN_CS_USER	FLAT_USER_CS
#define XEN_DS_USER	FLAT_USER_DS

#endif

#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__SEGMENT_H__ */

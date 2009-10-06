/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/config.h
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
#ifndef __KAXEN__CONFIG_H__
#define __KAXEN__CONFIG_H__

#ifndef CONFIG_WEDGE_KAXEN
#define CONFIG_WEDGE_KAXEN
#endif
#define CONFIG_WEDGE	kaxen

/* Virtual address space layout of the wedge's region.
 */
#define CONFIG_WEDGE_PGTAB_REGION	(1<<22)
#define CONFIG_WEDGE_P2M_REGION		(1<<22)
#define CONFIG_WEDGE_TMP_REGION		(10 * (1<<12))
#define CONFIG_WEDGE_XEN_SHARED		(1<<12)
#define CONFIG_WEDGE_WINDOW (CONFIG_WEDGE_PGTAB_REGION \
	+ CONFIG_WEDGE_P2M_REGION \
	+ CONFIG_WEDGE_TMP_REGION \
	+ CONFIG_WEDGE_XEN_SHARED)

#if defined(CONFIG_XEN_2_0)
# define XEN_EXTMMU_SHIFT		0

#elif defined(CONFIG_XEN_3_0)
# define XEN_EXTMMU_SHIFT		PAGE_BITS

/* Re-defines some of the Xen 2.0 things */
# define ARGS_PER_MULTICALL_ENTRY	8
# define ARGS_PER_EXTMMU_ENTRY		128
# define MMUEXT_INVLPG			MMUEXT_INVLPG_LOCAL
# define MMUEXT_TLB_FLUSH		MMUEXT_TLB_FLUSH_LOCAL
# define FLAT_GUESTOS_CS		FLAT_KERNEL_CS
# define FLAT_GUESTOS_DS		FLAT_KERNEL_DS

#endif

#endif	/* __KAXEN__CONFIG_H__ */

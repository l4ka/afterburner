/*********************************************************************
 *
 * Copyright (C) 2004-2006,  University of Karlsruhe
 *
 * File path:	l4ka-driver-reuse/kernel/pci/common.h
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
#ifndef __L4KA_DRIVER_REUSE__KERNEL__PCI__COMMON_H__
#define __L4KA_DRIVER_REUSE__KERNEL__PCI__COMMON_H__

#include <l4/types.h>

#ifndef TRUE
# define TRUE	1
#endif
#ifndef FALSE
# define FALSE	0
#endif

#define RAW(a)	((void *)((a).raw))

#define PARANOID(a)		a
#define ASSERT(a)		do { if(!(a)) { printk( PREFIX "assert failure %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__); L4_KDB_Enter("assert"); } } while(0)

#define dprintk(n,a...) do { if(L4VMpci_debug_level >= (n)) printk(a); } while(0)


extern int L4VMpci_debug_level;


#endif	/* __L4KA_DRIVER_REUSE__KERNEL__PCI__COMMON_H__ */

/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/config.h
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
 * $Id: config.h,v 1.9 2005/12/21 09:23:12 store_mrs Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__CONFIG_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__CONFIG_H__

#ifndef CONFIG_WEDGE_L4KA
#define CONFIG_WEDGE_L4KA
#endif
#define CONFIG_WEDGE	l4ka

#define CONFIG_UTCB_AREA_SIZE	(4096 * CONFIG_NR_VCPUS)
#define CONFIG_KIP_AREA_SIZE	(4096)

#if defined(CONFIG_L4KA_VMEXT) || defined(CONFIG_L4KA_HVM)
#define CONFIG_PRIO_DELTA_MONITOR       (0)
#define CONFIG_PRIO_DELTA_IRQ           (-1)
#define CONFIG_PRIO_DELTA_IRQ_HANDLER   (-2)
#define CONFIG_PRIO_DELTA_MAIN          (-3)
#define CONFIG_PRIO_DELTA_USER          (-4)
#else 
#define CONFIG_PRIO_DELTA_MONITOR       (0)
#define CONFIG_PRIO_DELTA_IRQ           (-1)
#define CONFIG_PRIO_DELTA_IRQ_HANDLER   (-1)
#define CONFIG_PRIO_DELTA_MAIN          (-3)
#define CONFIG_PRIO_DELTA_USER          (-5)
#endif

#if defined(CONFIG_L4KA_HVM)
#define DEFAULT_PAGE_BITS		PAGE_BITS
#else
#define DEFAULT_PAGE_BITS		PAGE_BITS
#endif

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__CONFIG_H__ */

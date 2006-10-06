/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     l4ka-resourcemon/resourcemon/resourcemon.h
 * Description:   General purpose support for the resourcemon.
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
#ifndef __L4KA_RESOURCEMON__RESOURCEMON__RESOURCEMON_H__
#define __L4KA_RESOURCEMON__RESOURCEMON__RESOURCEMON_H__

#include <common/ia32/page.h>

#include "resourcemon_idl_client.h"

#define RAW(t)          ((void *)((t).raw))

#define SPECIAL_MEM	MB(8)
#define TASK_LEN        MB(168U)
#define MAX_IRQS	256

//  Note: these must be consistent in terms of allignment!
# define KIP_LOCATION            (0x300000)
# define KIP_SIZE_BITS           (14)
# define UTCB_LOCATION           (0x304000)
# define UTCB_SIZE_BITS          (14)

extern void register_interface( guid_t guid, L4_ThreadId_t tid );

extern inline void set_tot_mem( L4_Word_t tot )
{
    extern L4_Word_t resourcemon_tot_mem;
    resourcemon_tot_mem = tot;
}

extern inline L4_Word_t get_tot_mem()
{
    extern L4_Word_t resourcemon_tot_mem;
    return resourcemon_tot_mem;
}

extern inline void set_max_phys_addr( L4_Word_t max )
{
    extern L4_Word_t resourcemon_max_phys_addr;
    resourcemon_max_phys_addr = max;
}

extern inline L4_Word_t get_max_phys_addr()
{
    extern L4_Word_t resourcemon_max_phys_addr;
    return resourcemon_max_phys_addr;
}

extern inline L4_Word_t get_resourcemon_end_addr()
{
    extern char _end[];
    return (L4_Word_t)_end;
}

#endif	/* __L4KA_RESOURCEMON__RESOURCEMON__RESOURCEMON_H__ */

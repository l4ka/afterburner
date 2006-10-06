/*********************************************************************
 *
 * Copyright (C) 2005-2006  University of Karlsruhe
 *
 * File path:     common/resourcemon.h
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
#ifndef __COMMON__RESOURCEMON_H__
#define __COMMON__RESOURCEMON_H__

#include <common/debug.h>

#include "hypervisor_idl_client.h"

extern IHypervisor_shared_t resourcemon_shared;

template <typename T>
word_t virt_to_dma( T virt )
{
    ASSERT( (word_t)virt > resourcemon_shared.link_vaddr );
    word_t offset = (word_t)virt - resourcemon_shared.link_vaddr;
    ASSERT( offset < resourcemon_shared.phys_size );

    word_t dma = offset + resourcemon_shared.phys_offset;
    ASSERT( dma < resourcemon_shared.phys_end );

    return dma;
}

template <typename T>
T dma_to_virt( word_t dma )
{
    ASSERT( dma < resourcemon_shared.phys_end );
    word_t offset = dma - resourcemon_shared.phys_offset;
    ASSERT( offset < resourcemon_shared.phys_size );
    return (T)(offset + resourcemon_shared.link_vaddr);
}

INLINE word_t get_max_prio()
{
    return resourcemon_shared.prio;
}

INLINE L4_ThreadId_t get_thread_server_tid()
{
    return resourcemon_shared.cpu[L4_ProcessorNo()].thread_server_tid;
}

INLINE L4_ThreadId_t get_locator_tid()
{
    return resourcemon_shared.cpu[L4_ProcessorNo()].locator_tid;
}

INLINE L4_ThreadId_t get_resourcemon_tid()
{
    return resourcemon_shared.cpu[L4_ProcessorNo()].hypervisor_tid;
}


extern bool resourcemon_register_service( guid_t guid, L4_ThreadId_t tid );
extern bool resourcemon_tid_to_space_id( L4_ThreadId_t tid, word_t *space_id );
extern bool resourcemon_continue_init();

class space_info_t
{
public:
    word_t space_id;
    word_t bus_start;
    word_t bus_size;
};

extern bool resourcemon_get_space_info( L4_ThreadId_t tid, space_info_t *info );

#endif	/* __COMMON__RESOURCEMON_H__ */

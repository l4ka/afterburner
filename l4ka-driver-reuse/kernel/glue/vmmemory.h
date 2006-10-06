/*********************************************************************
 *
 * Copyright (C) 2004-2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/vmmemory.h
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
#ifndef __L4KA_DRIVERS__GLUE__VMMEMORY_H__
#define __L4KA_DRIVERS__GLUE__VMMEMORY_H__

#include <linux/mm.h>
#include <linux/vmalloc.h>

typedef struct 
{
    L4_Word_t start;
    L4_Word_t actual_log2size;
    L4_Fpage_t fpage;
} L4VM_alligned_alloc_t;

extern int L4VM_fpage_alloc( L4_Word_t req_log2size, L4VM_alligned_alloc_t *);

extern inline void L4VM_fpage_dealloc( L4VM_alligned_alloc_t *info )
{
    L4_Word_t order = info->actual_log2size - PAGE_SHIFT;
    free_pages( info->start, order );
}

typedef struct 
{
    void *ioremap_addr;
    L4_Fpage_t fpage;
} L4VM_alligned_vmarea_t;

extern int L4VM_fpage_vmarea_get( L4_Word_t req_log2size, L4VM_alligned_vmarea_t * );
extern void L4VM_fpage_vmarea_release( L4VM_alligned_vmarea_t * );

typedef struct L4VM_client_space_info
{
    L4_Word_t space_id;
    L4_Word_t bus_start, bus_size;
    L4_Word_t refcnt;
    struct L4VM_client_space_info *next;
} L4VM_client_space_info_t;

extern int L4VM_tid_to_space_id(
	L4_ThreadId_t tid,
	L4_Word_t *space_id );

extern L4VM_client_space_info_t * L4VM_get_space_info( L4_Word_t space_id );
extern L4VM_client_space_info_t * L4VM_get_client_space_info( L4_ThreadId_t );


extern int L4VM_get_space_dma_info( L4_Word_t space_id, L4_Word_t *phys_start, L4_Word_t *phys_size );
extern int L4VM_get_client_dma_info( L4_ThreadId_t client_tid, L4_Word_t *phys_start, L4_Word_t *phys_size );

extern void L4VM_page_in( L4_Fpage_t, L4_Word_t page_size );

#endif	/* __L4KA_DRIVERS__GLUE__VMMEMORY_H__ */

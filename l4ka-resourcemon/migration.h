/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     migration.h
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
 * $Id$
 *                
 ********************************************************************/
#ifndef MIGRATION_H_
#define MIGRATION_H_

#include <l4/types.h>

//vm_t *do_clone_vm(L4_Word_t source_id);

typedef struct _VMInfo {
    // general VM info
	
    // contiguous physical memory region (paddr_len)
    L4_Word_t space_size;
    // The base address for this VM's contiguous memory region within the 
    // resourcemon
    L4_Word_t haddr_base;  
    // virtual address space offset
    L4_Word_t vaddr_offset;

    // wedge memory information
    L4_Word_t wedge_paddr; // physical start address
    L4_Word_t wedge_vaddr; // virtual start address
    L4_Word_t wedge_size; // size
	
    // shared data between the resourcemon and the VM
    // resides in the resourcemon's physical memory and
    // is mapped into the VM address space, so this area
    // must be copied separately
    static const L4_Word_t client_shared_size = sizeof(IResourcemon_shared_t) + 4096;
    
    L4_Word_t client_shared_area[client_shared_size];
    // client shared page vaddr from ELF
    L4_Word_t client_shared_vaddr;

    // KIP, as read from the ELF image
    // L4_Fpage_t source_kip_fp = source_vm->get_kip_fp();
    // L4_Word_t kip_start = L4_Address(source_kip_fp);
    // L4_Word_t kip_size = L4_Size(source_kip_fp);
    // this->kip_fp = L4_Fpage(kip_start, kip_size);
    L4_Word_t kip_start;
    L4_Word_t kip_size;

    // UTCB, as read from the ELF image
    // L4_Fpage_t source_utcb_fp = source_vm->get_utcb_fp();
    // L4_Word_t utcb_start = L4_Address(source_utcb_fp);
    // L4_Word_t utcb_size = L4_Size(source_utcb_fp);
    // this->utcb_fp = L4_Fpage(utcb_start, utcb_size);	
    L4_Word_t utcb_start;
    L4_Word_t utcb_size;

    // start and stack addresses
    L4_Word_t binary_stack_vaddr;
    L4_Word_t binary_entry_vaddr;

    // energy accounting info
} VMInfo;

#endif /*MIGRATION_H_*/

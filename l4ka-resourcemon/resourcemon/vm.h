/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/vm.h
 * Description:   Abstractions for virtual machines.
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
#ifndef __HYPERVISOR__INCLUDE__VM_H__
#define __HYPERVISOR__INCLUDE__VM_H__

#include <l4/kip.h>

#include <resourcemon/resourcemon.h>
#include "resourcemon_idl_server.h"
#include <common/bitmap.h>

#define MAX_VM	(16)

#define THREAD_SPACE_SIZE	(4096)
#define PRIO_IRQ		(254)
#define PRIO_ROOTSERVER		(255)

#define VM_DEVICE_PASSTHRU

class tid_space_t;
class vm_t;
class vm_allocator_t;

class tid_space_t
{
private:
    static L4_Word_t base_tid;

public:
    static void init()
    {
	// Space 0 is the resourcemon itself, and so the base TID starts
	// with the resourcemon's first TID.
	base_tid = L4_Myself().global.X.thread_no;
    }

    static L4_Word_t get_tid_start( L4_Word_t space_id )
    {
	return space_id * THREAD_SPACE_SIZE + base_tid;
    }

    static L4_Word_t tid_to_space_id( L4_ThreadId_t tid )
    {
	return (tid.global.X.thread_no - base_tid) / THREAD_SPACE_SIZE;
    }

    static L4_Word_t get_tid_space_size()
    {
	return THREAD_SPACE_SIZE;
    }
};


class vm_t
{
private:
    bool allocated;  // Is this VM structure allocated?
    bool device_access_enabled;
    bool client_dma_enabled; // Won't support clients which want DMA.
    L4_Word_t space_id;
    // The base address for this VM's contiguous memory region within the 
    // resourcemon.
    L4_Word_t haddr_base;  
    // The length of the VM's contiguous memory region.
    L4_Word_t paddr_len;
    // The VM can have an offset for its virtual address space.  In other
    // words, physical pages aren't allocated for the region of virtual
    // address space covered by this offset.
    L4_Word_t vaddr_offset;
    // The max prio of any virtual machine thread.
    L4_Word_t prio;

    L4_Word_t wedge_paddr;
    L4_Word_t wedge_vaddr;

    L4_Fpage_t kip_fp, utcb_fp;

    // virtual cpus
    L4_Word_t vcpu_count;

    IResourcemon_shared_t *client_shared;

    friend class vm_allocator_t;

private:
    bool elf_load( L4_Word_t elf_start );
    bool elf_section_describe( L4_Word_t elf_start, char *search_name,
	    L4_Word_t *addr, L4_Word_t *size );

    void shadow_special_memory();
    bool activate_thread();

public:

    void init( L4_Word_t space_id );

    bool init_mm( L4_Word_t size, L4_Word_t vaddr_offset, bool shadow_special,
	    L4_Word_t wedge_size, L4_Word_t wedge_paddr );

    void set_vaddr_offset( L4_Word_t new_offset )
    {
	this->vaddr_offset = new_offset;
	if( this->client_shared )
	    this->client_shared->link_vaddr = this->vaddr_offset;
    }

    L4_Word_t get_space_id()
	{ return this->space_id; }
    L4_Word_t get_space_size()
	{ return this->paddr_len; }
    L4_Word_t get_haddr_base()
	{ return this->haddr_base; }
    L4_Fpage_t get_kip_fp()
	{ return this->kip_fp; }
    L4_Fpage_t get_utcb_fp()
	{ return this->utcb_fp; }
    L4_Word_t get_prio()
	{ return this->prio; }

    L4_ThreadId_t get_first_tid()
    {
	L4_ThreadId_t tid;
	tid.global.X.thread_no = tid_space_t::get_tid_start( get_space_id() );
	tid.global.X.version = 2;
	return tid;
    }

    void enable_device_access() { 
	this->device_access_enabled = true;
	this->client_dma_enabled = true; 
	this->prio += 10;
    }
    bool has_device_access()
	{ return this->device_access_enabled; }
    void disable_client_dma_access()
	{ this->client_dma_enabled = false; }
    bool has_client_dma_access()
	{ return this->client_dma_enabled; }

    L4_Word_t get_device_low(L4_Word_t idx)
	{ if (idx < IResourcemon_max_devices) return this->client_shared->devices[idx].low; }
    L4_Word_t get_device_high(L4_Word_t idx)
	{ if (idx < IResourcemon_max_devices) return this->client_shared->devices[idx].high; }

    
    bool client_paddr_to_haddr( L4_Word_t paddr, L4_Word_t *haddr )
    {
	if( paddr < this->paddr_len )
	{
	    *haddr = paddr + this->haddr_base;
	    return true;
	}
	
	return false;
    }

    bool client_paddr_to_vaddr( L4_Word_t paddr, L4_Word_t *vaddr )
    {
	if( paddr >= this->paddr_len )
	    return false;
	if( paddr >= wedge_paddr && (!client_shared || (paddr < wedge_paddr+client_shared->wedge_phys_size)) )
	    *vaddr = paddr + this->wedge_vaddr;
	else
	    *vaddr = paddr + this->vaddr_offset;

	return true;
    }

    bool client_vaddr_to_haddr( L4_Word_t vaddr, L4_Word_t *haddr );
    bool client_vaddr_to_paddr( L4_Word_t vaddr, L4_Word_t *paddr );

    bool install_elf_binary( L4_Word_t elf_start );
    bool install_module( L4_Word_t end, L4_Word_t haddr_start, L4_Word_t haddr_end, const char *cmdline );
    bool install_ramdisk( L4_Word_t haddr_start, L4_Word_t haddr_end );
    bool init_client_shared( const char *cmdline );
    bool start_vm();

    void set_memballoon( L4_Word_t size ) 
    {
	if ( client_shared && size < this->get_space_size() )
	    client_shared->mem_balloon = size;
    }

    void set_vcpu_count( L4_Word_t count )
    {
	this->vcpu_count = count;
    }

#if defined(cfg_l4ka_vmextensions)
    void set_vtimer_tid( L4_Word_t cpu, L4_ThreadId_t tid )
    {
	this->client_shared->cpu[cpu].vtimer_tid = tid;
    }
    void set_vtimer_irq_pending( L4_Word_t cpu, L4_Word_t handler_idx)
    {
	this->client_shared->cpu[cpu].vtimer_irq_pending = (1 << handler_idx);
    }
#endif

    
    /* Infrequently accessed data, and large blocks of data.
     */
private:
    // Information about the binary extracted from the ELF header.
    L4_Word_t binary_entry_vaddr, binary_start_vaddr, binary_end_vaddr;
    L4_Word_t binary_stack_vaddr;
    L4_Word_t elf_entry_vaddr;
    // A bitmap to track the threads started by the VM.
    bitmap_t<THREAD_SPACE_SIZE> tid_bitmap;
    // A bitmap to track the interrupts associated to the VM.
    bitmap_t<MAX_IRQS> irq_bitmap;
};


class vm_allocator_t
{
private:
    vm_t vm_list[MAX_VM];

public:
    void init();
    vm_t * allocate_vm();

    void deallocate_vm( vm_t *vm )
    {
	vm->allocated = false;
    }

    vm_t * tid_to_vm( L4_ThreadId_t tid )
    {
	L4_Word_t space_id = tid_space_t::tid_to_space_id( tid );
	if( (space_id >= MAX_VM) || !this->vm_list[space_id].allocated )
	    return NULL;
	return &this->vm_list[space_id];
    }

    vm_t * get_resourcemon_vm()
    {
	// The first vm corresponds to the resourcemon.
	return &vm_list[0];
    }

    vm_t * space_id_to_vm( L4_Word_t space_id )
    {
	if( space_id < MAX_VM )
	    return &this->vm_list[ space_id ];
	return NULL;
    }
};


extern inline vm_allocator_t * get_vm_allocator()
{
    extern vm_allocator_t vm_allocator;
    return &vm_allocator;
}

#endif	/* __HYPERVISOR__INCLUDE__VM_H__ */

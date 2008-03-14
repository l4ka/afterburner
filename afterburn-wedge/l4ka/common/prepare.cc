/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/ia32/prepare.cc
 * Description:   Prepare the guest OS binary for execution in our environment.
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

#include INC_WEDGE(backend.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(vcpulocal.h)

#include <bind.h>
#include <elfsimple.h>

extern bool frontend_elf_rewrite( elf_ehdr_t *elf, word_t vaddr_offset, bool module );
extern void bind_guest_uaccess_fault_handler();

bool
backend_load_vcpu( backend_vcpu_init_t *init_info )
{
    bool kernel_loaded = false;
    elf_ehdr_t *elf;
    vcpu_t &vcpu = get_vcpu();

    for( word_t idx = 0; idx < resourcemon_shared.module_count; idx++ )
    {
	IResourcemon_shared_module_t &mod = resourcemon_shared.modules[idx];
	if( NULL != (elf = elf_is_valid(mod.vm_offset)) )
	{
	    // Install the binary in the first 64MB.
	    // Convert to paddr.
	    if( elf->entry >= MB(64) )
		init_info->entry_ip = (MB(64)-1) & elf->entry;
	    else if( elf->entry >= MB(8) )
		init_info->entry_ip = (MB(8)-1) & elf->entry;
	    else
		PANIC( "The ELF binary is linked below 8MB: %x ", elf->entry);
	    // Infer the guest's vaddr offset from its ELF entry address.
	    vcpu.set_kernel_vaddr( elf->entry - init_info->entry_ip );

	    // Look for overlap with the wedge.
	    word_t phys_min, phys_max;
	    elf->get_phys_max_min( vcpu.get_kernel_vaddr(), phys_max, 
		    phys_min );
	    if( phys_max > vcpu.get_wedge_paddr() ) {
		printf( "VM overlaps with the wedge.\n");
		return false;
	    }

	    // Load the kernel.
	    elf->load_phys( vcpu.get_kernel_vaddr() );

	    // Patchup the binary.
	    if(!frontend_elf_rewrite(elf, vcpu.get_kernel_vaddr(), false))
		return false;
	    bind_guest_uaccess_fault_handler();

	    kernel_loaded = true;
	}
	else 
	{
	    // Not an ELF, so must be a ramdisk.
	    // Maintain compatibility with the old init code.
	    resourcemon_shared.ramdisk_start = mod.vm_offset;
	    resourcemon_shared.ramdisk_size = mod.size;
	}
    }

    if( !kernel_loaded )
	printf( "Unable to find a valid kernel.\n");
    return kernel_loaded;
}


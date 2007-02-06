/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/vm.cc
 * Description:   The virtual machine abstraction.
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

#include <l4/types.h>
#include <l4/sigma0.h>
#include <l4/schedule.h>

#include <resourcemon/vm.h>
#include <common/elf.h>
#include <common/basics.h>
#include <common/debug.h>
#include <common/string.h>
#include <common/console.h>

L4_Word_t tid_space_t::base_tid;

vm_t * vm_allocator_t::allocate_vm()
{
    // Return the first available vm, and change its status to allocated.
    for( L4_Word_t i = 0; i < MAX_VM; i++ )
	if( this->vm_list[i].allocated == false )
	{
	    vm_t *vm = &this->vm_list[i];
	    vm->allocated = true;
	    return vm;
	}

    return NULL;
}

void vm_allocator_t::init()
{
    // Init all of the vm instances.
    for( L4_Word_t i = 0; i < MAX_VM; i++ )
	this->vm_list[i].init( i );

    // Reserve the first vm for the resourcemon.
    vm_t *vm = this->get_resourcemon_vm();
    vm->allocated = true;
    vm->init_mm( get_resourcemon_end_addr(), MB(0), false, 0, 0 );
}

void vm_t::init( L4_Word_t new_space_id )
{
    this->allocated = false;
    this->device_access_enabled = false;
    this->space_id = new_space_id;
    this->haddr_base = 0;
    this->paddr_len = 0;
    this->vaddr_offset = 0;
    this->vcpu_count = 1;
    this->pcpu_count = 1;
    this->prio = 102;
}

bool vm_t::init_mm( L4_Word_t size, L4_Word_t new_vaddr_offset, bool shadow_special, L4_Word_t init_wedge_size, L4_Word_t init_wedge_paddr )
{
    size = round_up( size, SUPER_PAGE_SIZE );

    // Look for a block of contiguous memory to hold our VM, unless
    // it is for the resourcemon (space_id == 0), and then take the requested
    // size at addr 0.
    L4_Word_t start_addr = 0, end_addr;
    bool found = (get_space_id() == 0);
    while( !found )
    {
	found = true;
	end_addr = start_addr + size;

	// Look for conflicts with other virtual machines.
	for( L4_Word_t id = 0; id < MAX_VM; id++ )
	{
	    vm_t *vm = get_vm_allocator()->space_id_to_vm(id);
	    if( vm->get_space_size() == 0 )
		continue;
	    if( (end_addr <= vm->get_haddr_base()) ||
		    (start_addr >= (vm->get_haddr_base() + vm->get_space_size())) )
		continue;
	    // Start looking in the memory following this vm.
	    found = false;
	    start_addr = vm->get_haddr_base() + vm->get_space_size();
	    start_addr = round_up( start_addr, MB(4) );
	    break;
	}

	// Look for conflicts with KIP memory descriptors.
	if( found )
	{
	    L4_Word_t next;
	    while( kip_conflict(start_addr, size, &next) )
	    {
		start_addr = round_up( next, MB(4) );
		found = false;		// validate against the VM list.
		if( start_addr == 0 )
		    return false;	// wrap-around
	    }
	}
    }

    if( !found )
	return false; // Unable to find a contiguous block of memory.

    if( (start_addr + size - 1) >= get_max_phys_addr() )
	return false; // Not enough memory.

    this->paddr_len = size;
    this->vaddr_offset = new_vaddr_offset;
    this->haddr_base = start_addr;

    this->wedge_paddr = init_wedge_paddr;
    this->wedge_vaddr = 0;

    hout << "Creating VM ID " << this->get_space_id()
	 << ", at " << (void *)this->haddr_base 
	 << ", size " << (void *)this->paddr_len
	 << '\n';
    hout << "Thread space, first TID: " << this->get_first_tid()
	 << ", number of threads: " << tid_space_t::get_tid_space_size()
	 << '\n';

    if( shadow_special )
    {
        memset( (void *)this->get_haddr_base(), 0, this->get_space_size() );
	this->shadow_special_memory();
    }

    return true;
}

void vm_t::shadow_special_memory()
{
    L4_Word_t tot = 0;

    zero_mem( (void *)this->get_haddr_base(), this->get_space_size() );
    return;

    // If we execute this copy after the resourcemon has grabbed all physical
    // memory, then the destination areas can still use large page
    // sizes.
    for( L4_Word_t page = 0; page < SPECIAL_MEM; page += PAGE_SIZE )
    {
	L4_Word_t haddr;
	L4_Fpage_t req_fp = L4_FpageLog2( page, PAGE_BITS );
	L4_Fpage_t rcv_fp = L4_Sigma0_GetPage( L4_Pager(), req_fp, req_fp );
	if( !client_paddr_to_haddr(page, &haddr) )
	    continue;

	if( L4_IsNilFpage(rcv_fp) )
	    memset( (void *)haddr, 0, PAGE_SIZE );
	else
	{
	    memcpy( (void *)haddr, (void *)page, PAGE_SIZE );
	    tot++;
	}
    }

    if( tot == 0 )
    {
	hout << "Error: something broken, no special platform memory!\n";
#if defined(__i386__)
	L4_KDB_Enter( "notice me" );
#endif
    }
}

bool vm_t::client_vaddr_to_haddr( L4_Word_t vaddr, L4_Word_t *haddr )
{
    if( wedge_paddr && (vaddr >= wedge_vaddr) )
    {
	return this->client_paddr_to_haddr( vaddr - wedge_vaddr + wedge_paddr, 
		haddr );
    }

    return this->client_paddr_to_haddr( vaddr - this->vaddr_offset, haddr );
}

bool vm_t::client_vaddr_to_paddr( L4_Word_t vaddr, L4_Word_t *paddr )
{
    if( vaddr >= this->vaddr_offset )
    {
	*paddr = vaddr - this->vaddr_offset;
	return true;
    }
    return false;
}

bool vm_t::elf_section_describe( 
	L4_Word_t file_start, 
	char *search_name, 
	L4_Word_t *addr,
	L4_Word_t *size )
{
    elf_ehdr_t *ehdr = (elf_ehdr_t *)file_start;
    elf_shdr_t *shdr, *shdr_list, *str_hdr;
    L4_Word_t s_cnt;
    char *section_name;

    shdr_list = (elf_shdr_t *)(file_start + ehdr->shoff);
    str_hdr = &shdr_list[ ehdr->shstrndx ];
    for( s_cnt = 0; s_cnt < ehdr->shnum; s_cnt++ )
    {
	shdr = &shdr_list[ s_cnt ];
	section_name = (char *)
	    (shdr->name + file_start + str_hdr->offset);

	if( !strcmp(section_name, search_name) )
	{
	    *addr = shdr->addr;
	    *size = shdr->size;
	    return true;
	}
    }

    return false;
}

/**
 * ELF-load an ELF memory image
 *
 * @param file_start    First byte of source ELF image in memory
 * @param memory_start  Pointer to address of first byte of loaded image
 * @param memory_end    Pointer to address of first byte behind loaded image
 * @param entry         Pointer to address of entry point
 *
 * This function ELF-loads an ELF image that is located in memory.  It
 * interprets the program header table and copies all segments marked
 * as load. It stores the lowest and highest+1 address used by the
 * loaded image as well as the entry point into caller-provided memory
 * locations.
 *
 * @returns false if ELF loading failed, true otherwise.
 */
bool vm_t::elf_load( L4_Word_t file_start )
{
    // Pointer to ELF file header
    elf_ehdr_t* eh = (elf_ehdr_t*) file_start;

    if( !elf_is_valid(file_start) || (eh->type != 2) )
    {
	hout << "Error: invalid ELF binary.\n";
	return false;
    }

    // Is the address of the PHDR table valid?
    if (eh->phoff == 0)
    {
        // No. Bail out
	hout << "Error: ELF binary has wrong PHDR table offset.\n";
        return false;
    }

    if( wedge_paddr ) {
	wedge_vaddr = eh->entry - (eh->entry & (MB(64)-1));
	hout << "Wedge virt offset " << (void *)wedge_vaddr
	    << ", phys offset " << (void *)wedge_paddr << '\n';
    }

    hout << "ELF entry virtual address: " << (void *)eh->entry << '\n';

    // Locals to find the enclosing memory range of the loaded file
    L4_Word_t max_addr =  0UL;
    L4_Word_t min_addr = ~0UL;

    // Walk the program header table
    for (L4_Word_t i = 0; i < eh->phnum; i++)
    {
        // Locate the entry in the program header table
        elf_phdr_t* ph = (elf_phdr_t*) 
	    (file_start + eh->phoff + eh->phentsize * i);
        
        // Is it to be loaded?
        if (ph->type == PT_LOAD)
        {
	    L4_Word_t haddr, haddr2;
	    if( !client_vaddr_to_haddr(ph->vaddr, &haddr) ||
		    !client_vaddr_to_haddr(ph->vaddr+ph->msize-1, &haddr2) )
	    {
		hout << "Error: ELF file doesn't fit!\n";
		return false;
	    }
	    hout << "  Source " << (void *)(file_start + ph->offset)
		 << ", size " << (void *)ph->fsize
		 << " --> resourcemon address " << (void *)haddr
		 << ", VM address " << (void *)ph->vaddr
		 << '\n';

            // Copy bytes from "file" to memory - load address
            memcpy((void *)haddr,
		    (void *)(file_start + ph->offset), ph->fsize);
	    // Zero "non-file" bytes
	    memset ((void *)(haddr + ph->fsize), 
		    0, ph->msize - ph->fsize);
            // Update min and max
            min_addr <?= ph->vaddr;
            max_addr >?= ph->vaddr + (ph->msize >? ph->fsize);
        }
        
    }
    // Write back the values into the caller-provided locations
    this->binary_start_vaddr = min_addr;
    this->binary_end_vaddr = max_addr;
    this->binary_entry_vaddr = this->elf_entry_vaddr = eh->entry;
    this->binary_stack_vaddr = 0;

    // Signal successful loading
    return true;
}


bool vm_t::install_elf_binary( L4_Word_t elf_start )
{
    // Install the elf binary.
    if( !elf_load(elf_start) )
	return false;

    /* Figure out where to install the kip.
     */
    L4_Word_t kip_start, kip_size;
    if( !elf_section_describe(elf_start, ".l4kip", &kip_start, &kip_size) )
    {
	kip_start = KIP_LOCATION;
	kip_size = 1 << KIP_SIZE_BITS;
	if( (kip_start + kip_size) > this->binary_start_vaddr )
	{
	    hout << "Error: unable to find a decent KIP location.\n";
	    hout << " KIP start at " << (void *)kip_start
		 << ", size " << (void *)kip_size
		 << ", end " << (void *)(kip_start + kip_size)
		 << ", binary VM vaddr start " 
		 << (void *)this->binary_start_vaddr
		 << '\n';
	    return false;
	}
    }

    this->kip_fp = L4_Fpage(kip_start, kip_size);

    if( L4_Address(this->kip_fp) != kip_start )
    {
	hout << "Error: the KIP area is misaligned, aborting.\n";
	return false;
    }

    /* Figure out where to install the utcb.
     */
    L4_Word_t utcb_start, utcb_size;
    if( !elf_section_describe(elf_start, ".l4utcb", &utcb_start, &utcb_size) )
    {
	utcb_start = UTCB_LOCATION;
	utcb_size = 1UL << UTCB_SIZE_BITS;
	if( (utcb_start + utcb_size) > this->binary_start_vaddr )
	{
	    hout << "Error: unable to find a decent UTCB location.\n";
	    return false;
	}
    }

    this->utcb_fp = L4_Fpage(utcb_start, utcb_size);

    if( L4_Address(this->utcb_fp) != utcb_start )
    {
	hout << "Error: the UTCB section is misaligned, aborting.\n";
	hout << "  UTCB start: " << (void *)utcb_start
	     << ", UTCB size: " << (void *)utcb_size
	     << '\n';
	return false;
    }

    /* Locate the client shared data structure.
     */
    L4_Word_t shared_start, shared_size;
    if( elf_section_describe(elf_start, ".resourcemon", &shared_start, &shared_size) )
    {
	if( shared_size < sizeof(IResourcemon_shared_t) )
	{
	    hout << "Error: the .resourcemon ELF section is too small; please "
		"ensure that the binary is compatible with this resourcemon.\n";
	    return false;
	}

	hout << "Hypervisor shared page at VM address " 
	     << (void *)shared_start << ", size " << (void *)shared_size
	     << '\n';
	L4_Word_t end_addr;
	bool result = client_vaddr_to_haddr( shared_start,
		(L4_Word_t *)&this->client_shared );
	result |= client_vaddr_to_haddr( (shared_start + shared_size - 1),
		&end_addr );
	if( !result )
	{
	    hout << "Error: the .resourcemon elf section doesn't fit within"
		    " allocated memory for the virtual machine, aborting.\n";
	    return false;
	}
    }
    else
	this->client_shared = NULL;

    /* Look for an alternate start address.
     */
    L4_Word_t alternate_start, alternate_size;
    if( elf_section_describe(elf_start, ".resourcemon.startup", &alternate_start, &alternate_size) )
    {
	if( alternate_size < sizeof(IResourcemon_startup_config_t) ) {
	    hout << "Error: version mismatch for the .resourcemon.startup "
		    "ELF section.\nPlease ensure that the binary is compatible "
		    "with this version of the resourcemon.\n";
	    return false;
	}

	IResourcemon_startup_config_t *alternate;
	L4_Word_t end_addr;
	bool result = client_vaddr_to_haddr( alternate_start,
		(L4_Word_t *)&alternate );
	result |= client_vaddr_to_haddr( (alternate_start + alternate_size - 1),
		&end_addr );
	if( !result )
	{
	    hout << "Error: the .resourcemon.startup ELF section doesn't fit "
		    "within allocated memory for the virtual machine, "
		    "aborting.\n";
	    return false;
	}

	this->binary_entry_vaddr = alternate->start_ip;
	this->binary_stack_vaddr = alternate->start_sp;

	hout << "Entry override, IP " << (void *)this->binary_entry_vaddr
	     << ", SP " << (void *)this->binary_stack_vaddr << '\n';
    }

    return true;
}

bool vm_t::init_client_shared( const char *cmdline )
{
    if( !client_shared )
    {
	hout << "Error: unable to locate the VM's shared resourcemon page.\n";
	return false;
    }

    if( client_shared->version != IResourcemon_version )
    {
	hout << "Error: the binary uses a different version of the "
	        "resourcemons's IDL file:\n"
	     << "  resourcemon's version: " << IResourcemon_version
	     << ", binary's version: " << client_shared->version << '\n';
	return false;
    }
    if( client_shared->cpu_cnt > IResourcemon_max_cpus )
    {
	hout << "Error: the binary expects " << client_shared->cpu_cnt
	     << " CPUS, but the resourcemon's IDL file supports "
	     << IResourcemon_max_cpus << '\n';
	return false;
    }

    L4_Word_t l4_cpu_cnt = L4_NumProcessors(L4_GetKernelInterface());
    if( l4_cpu_cnt > IResourcemon_max_cpus )
    {
	hout << "Error: " << l4_cpu_cnt << " L4 CPUs exceeds the "
	     << IResourcemon_max_cpus << " CPUs supported by the resourcemon.\n";
	return false;
    }
    for( L4_Word_t cpu = 0; cpu < l4_cpu_cnt; cpu++ )
    {
	client_shared->cpu[cpu].locator_tid = L4_Myself();
	client_shared->cpu[cpu].resourcemon_tid = L4_Myself();
	client_shared->cpu[cpu].thread_server_tid = L4_Myself();
	client_shared->cpu[cpu].time_balloon = 0;
    }

    client_shared->vcpu_count = this->vcpu_count;
    client_shared->pcpu_count = (this->pcpu_count && this->pcpu_count < l4_cpu_cnt) ?
	this->pcpu_count : l4_cpu_cnt;
    
    // evenly distribute the vcpus
    for ( L4_Word_t vcpu = 0; vcpu < IResourcemon_max_cpus; vcpu++ )
	client_shared->vcpu_to_l4cpu[vcpu] = vcpu % client_shared->pcpu_count;

    client_shared->ramdisk_start = 0;
    client_shared->ramdisk_size = 0;
    client_shared->module_count = 0;

    client_shared->thread_space_start =
	tid_space_t::get_tid_start( this->get_space_id() );
    client_shared->thread_space_len =
	tid_space_t::get_tid_space_size();

    client_shared->utcb_fpage = this->get_utcb_fp();
    client_shared->kip_fpage  = this->get_kip_fp();

    if (l4_has_iommu())
	client_shared->phys_offset = 0;
    else
	client_shared->phys_offset = this->get_haddr_base();

    client_shared->phys_size = this->get_space_size();
    if( this->has_device_access() && this->has_client_dma_access() )
	client_shared->phys_end = get_max_phys_addr();
    else
        client_shared->phys_end = this->get_space_size() - 1;

    client_shared->link_vaddr = this->vaddr_offset;
    client_shared->entry_ip = this->elf_entry_vaddr;
    client_shared->entry_sp = 0;

    client_shared->prio = this->get_prio();
    client_shared->mem_balloon = 0;

    if( cmdline )
    {
	if( (unsigned)strlen(cmdline) >= sizeof(client_shared->cmdline) )
	{
	    hout << "Error: the command line for the VM is too long.\n";
	    return false;
	}

	strncpy( client_shared->cmdline, cmdline,
		 sizeof(client_shared->cmdline) );
    }

    else
	*client_shared->cmdline = '\0';


#if defined(VM_DEVICE_PASSTHRU)
    L4_KernelInterfacePage_t * kip = 
	(L4_KernelInterfacePage_t *)L4_GetKernelInterface();
    
    L4_Word_t d=0;
    
    for( L4_Word_t i = 0; i < L4_NumMemoryDescriptors(kip); i++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc(kip, i);
	
	if(((L4_Type(mdesc) & 0xF) == L4_ArchitectureSpecificMemoryType))
	{
	    client_shared->devices[d].low = L4_Low(mdesc);
	    client_shared->devices[d].high = L4_High(mdesc);
	    d++;
	}
	if(((L4_Type(mdesc) & 0xF) == L4_ArchitectureSpecificMemoryType))
	
	if (d >= IResourcemon_max_devices)
	{
	    hout << "Could not register all device memory regions" 
		 << "for passthru access (" 
		 << d << ">" << IResourcemon_max_devices << ")\n";
	    break;
	}
    }
    
    if (d >= IResourcemon_max_devices)
    {
	hout << "Could not register all device memory regions" 
	     << "for passthru access (" 
	     << d << ">" << IResourcemon_max_devices << ")\n";
    }
    else
    {
	client_shared->devices[d].low = get_max_phys_addr();
	client_shared->devices[d].high = 0xffffffff;
    }
#endif
    
    return true;
}

bool vm_t::start_vm()
{
    L4_ThreadId_t tid, scheduler, pager;

    tid = this->get_first_tid();
    scheduler = tid;
    pager = L4_Myself();

    hout << "KIP at " << (void *)L4_Address(this->kip_fp)
	 << ", size " << (void *)L4_Size(this->kip_fp) << '\n';
    hout << "UTCB at " << (void *)L4_Address(this->utcb_fp)
	 << ", size " << (void *)L4_Size(this->utcb_fp) << '\n';
    hout << "Main thread TID: " << tid << '\n';

    if( !L4_ThreadControl(tid, tid, L4_Myself(), L4_nilthread, (void *)L4_Address(utcb_fp)) )
    {
	hout << "Error: failure creating first thread, TID " << tid
	     << ", scheduler TID " << scheduler 
	     << ", L4 error code " << L4_ErrorCode() << '\n';
	return false;
    }

    L4_Word_t dummy, control = 0;

    if (this->has_device_access() && l4_has_iommu())
    {
	static int num;
	if (num++ == 0){
	    control = (1<<30 | L4_TimePeriod(10 * 1000).raw);
	}
	else {
	    control = (1<<30 | L4_TimePeriod(10 * 1000).raw);
	}
    }

    if( !L4_SpaceControl(tid, control, this->kip_fp, this->utcb_fp, L4_nilthread, &dummy) )
    {
	hout << "Error: failure creating space, TID " << tid
	     << ", L4 error code " << L4_ErrorCode() << '\n';
	goto err_space_control;
    }

    // Set the thread's priority.
    if( !L4_Set_Priority(tid, this->get_prio()) )
    {
	hout << "Error: failure setting VM's priority, TID " << tid
	     << ", L4 error code " << L4_ErrorCode() << '\n';
	goto err_priority;
    }

#if defined(cfg_l4ka_vmextensions)
    // Set the thread's timeslice to never
    if( !L4_Set_Timeslice(tid, L4_Never, L4_Never) )
    {
	hout << "Error: failure setting VM's" 
	     << " timeslice to " << L4_Never.raw 
	     << " TID " << tid
	     << " L4 error code " << L4_ErrorCode() << '\n';
	goto err_priority;
    }
#endif

    // Make the thread valid.
    if( !L4_ThreadControl(tid, tid, scheduler, pager, (void *)-1UL) )
    {
	hout << "Error: failure starting thread, TID " << tid
	     << ", L4 error code " << L4_ErrorCode() << '\n';
	goto err_valid;
    }

    // Enable smallspaces for device drivers.
#if defined(cfg_smallspaces)
    if( l4_has_smallspaces() )
    {
	L4_Word_t control;
	if( this->has_device_access() )
	    control = (1 << 31) | L4_SmallSpace(0,64);
	else
	    control = (1 << 31) | L4_SmallSpace(1,256);
	if( !L4_SpaceControl(tid, control, L4_Nilpage, L4_Nilpage, L4_nilthread, &dummy) )
	    hout << "Warning: unable to activate a smallspace.\n";
	else
	    hout << "Small space establishing.\n";
	L4_KDB_Enter("small");
    }
    else
	hout << "No small spaces configured in the kernel.\n";
#endif

    // Start the thread running.
    if( !this->activate_thread() )
    {
	hout << "Error: failure making the thread runnable, TID "
	     << tid << '\n';
	goto err_activate;
    }

    return true;

err_priority:
err_space_control:
err_valid:
err_activate:
    // Delete the thread and space.
    L4_ThreadControl(tid, L4_nilthread, L4_nilthread, L4_nilthread, (void *)-1UL );
    return false;
}

bool vm_t::activate_thread()
{
//    L4_KDB_Enter( "starting VM" );
    L4_Msg_t msg;
    L4_Clear( &msg );
    L4_Append( &msg, this->binary_entry_vaddr );		// ip
    L4_Append( &msg, this->binary_stack_vaddr );		// sp
    L4_Load( &msg );

//    hprintf (1 , PREFIX "starting_linux thread @ 0x%lx \n",this->binary_entry_vaddr);
//    L4_KDB_Enter("activate_thread::thread");

    L4_MsgTag_t tag = L4_Send( this->get_first_tid() );

    return L4_IpcSucceeded(tag);
}

bool vm_t::install_ramdisk( L4_Word_t rd_start, L4_Word_t rd_end )
{
    // Too big?
    L4_Word_t size = rd_end - rd_start;
    if( size > this->get_space_size() )
	return false;

    // Too big?
    L4_Word_t target_paddr = this->get_space_size() - size;
    if( target_paddr < PAGE_SIZE )
	return false;
    target_paddr = (target_paddr - PAGE_SIZE) & PAGE_MASK;

    // Convert to virtual
    L4_Word_t target_vaddr;
    if( !client_paddr_to_vaddr(target_paddr, &target_vaddr) )
	return false;

    // Overlaps with the elf binary?
    if( is_region_conflict(target_vaddr, target_vaddr+size,
		this->binary_start_vaddr, this->binary_end_vaddr) )
	return false;

    // Overlaps with the kip or utcb?
    if( is_fpage_conflict(this->kip_fp, target_vaddr, target_vaddr+size) )
	return false;
    if( is_fpage_conflict(this->utcb_fp, target_vaddr, target_vaddr+size) )
	return false;

    // Can we translate the addresses?
    L4_Word_t haddr, haddr2;
    if( !client_paddr_to_haddr(target_paddr, &haddr) ||
	    !client_paddr_to_haddr(target_paddr + size - 1, &haddr2) )
	return false;

    hout << "Installing RAMDISK at VM address " << (void *)target_vaddr
	 << ", size " << (void *)size << '\n';

    memcpy( (void *)haddr, (void *)rd_start, size );

    // Update the client shared information.
    if( this->client_shared )
    {
	this->client_shared->ramdisk_start = target_vaddr;
	this->client_shared->ramdisk_size = size;
    }

    return true;
}

bool vm_t::install_module( L4_Word_t ceiling, L4_Word_t haddr_start, L4_Word_t haddr_end, const char *cmdline )
{
    // Too big?
    L4_Word_t size = haddr_end - haddr_start;
    if( size > ceiling ) {
	hout << "Module to big, size " << (void *)size
	     << ", ceiling " << (void *)ceiling << '\n';
	return false;
    }

    L4_Word_t target_paddr = (ceiling - size) & PAGE_MASK;

    // Convert to virtual
    L4_Word_t target_vaddr;
    if( !client_paddr_to_vaddr(target_paddr, &target_vaddr) )
	return false;

    // Overlaps with the elf binary?
    if( is_region_conflict(target_vaddr, target_vaddr+size,
		this->binary_start_vaddr, this->binary_end_vaddr) )
    {
	hout << "Module overlaps with the ELF binary.\n";
	return false;
    }

    // Overlaps with the kip or utcb?
    if( is_fpage_conflict(this->kip_fp, target_vaddr, target_vaddr+size) ) {
	hout << "Module overlaps with the KIP.\n";
	return false;
    }
    if( is_fpage_conflict(this->utcb_fp, target_vaddr, target_vaddr+size) ) {
	hout << "Module overlaps with the UTCB area.\n";
	return false;
    }

    // Can we translate the addresses?
    L4_Word_t haddr, haddr2;
    if( !client_paddr_to_haddr(target_paddr, &haddr) ||
	    !client_paddr_to_haddr(target_paddr + size - 1, &haddr2) )
	return false;

    hout << "Installing module at VM phys address " << (void *)target_paddr
	 << ", size " << (void *)size << '\n';

    memcpy( (void *)haddr, (void *)haddr_start, size );

    if( this->client_shared ) {
	L4_Word_t idx = this->client_shared->module_count;
	if( idx >= IResourcemon_max_modules ) {
	    hout << "Too many modules (hard coded limit).\n";
	    return false;
	}
	this->client_shared->modules[ idx ].vm_offset = target_paddr;
	this->client_shared->modules[ idx ].size = size;

	if( (unsigned)strlen(cmdline) >= 
		sizeof(this->client_shared->modules[idx].cmdline) )
	{
	    hout << "Error: the command line for the VM is too long.\n";
	    return false;
	}
	strncpy( this->client_shared->modules[idx].cmdline, cmdline,
		sizeof(this->client_shared->modules[idx].cmdline) );

	this->client_shared->module_count++;
    }

    return true;
}


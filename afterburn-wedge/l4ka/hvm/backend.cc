/*********************************************************************
 *                
 * Copyright (C) 2007-2008,  Karlsruhe University
 *                
 * File path:     backend.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <debug.h>
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <string.h>
#include <elfsimple.h>
#include <device/portio.h>

#include INC_ARCH(cpu.h)
#include INC_ARCH(ia32.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(module_manager.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(hvm_vmx.h)

extern word_t afterburn_c_runtime_init;

extern unsigned char _binary_rombios_bin_start[];
extern unsigned char _binary_rombios_bin_end[];
extern unsigned char _binary_vgabios_bin_start[];
extern unsigned char _binary_vgabios_bin_end[];

extern bool vm8086_interrupt_emulation(word_t vector, bool hw);

bool backend_sync_deliver_vector( L4_Word_t vector, bool old_int_state, bool use_error_code, L4_Word_t error_code )
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if( get_cpu().cr0.real_mode() ) 
    {
	// Real mode, emulate interrupt injection
	return vm8086_interrupt_emulation(vector, true);
    }

    hvm_vmx_int_t exc_info;
    exc_info.raw = 0;
    exc_info.vector = vector;
    exc_info.type = hvm_vmx_int_t::ext_int;
    exc_info.err_code_valid = use_error_code;
    exc_info.valid = 1;
    
    vcpu_mrs->append_exc_item(exc_info.raw, error_code, vcpu_mrs->hvm.ilen);

    printf("backend_sync_deliver_vector %d err %d\n", vector, error_code); 
    DEBUGGER_ENTER("UNTESTED INJECT IRQ");


    return true;
}




// signal to vcpu that we would like to deliver an interrupt
bool backend_async_irq_deliver( intlogic_t &intlogic )
{
    word_t vector, irq;
    
    vcpu_t vcpu = get_vcpu();
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    
    // are we already waiting for an interrupt window exit
    if( vcpu.interrupt_window_exit )
	return false;
    
    if( !intlogic.pending_vector( vector, irq ) )
	return false;

    flags_t eflags;
    hvm_vmx_gs_ias_t ias;
    ias.raw = 0;

    eflags.x.raw = vcpu_mrs->gpregs_item.regs.eflags; 

    if (eflags.interrupts_enabled())
    {
	// Get Interruptbility state 
	vcpu_mrs->append_nonreg_item(L4_CTRLXFER_NONREGS_INT, 0);
	vcpu_mrs->load();
	L4_ReadCtrlXferItems(vcpu.main_ltid);
	
	printf("unimplemented exregs store nonreg item");
	UNIMPLEMENTED();
	//L4_StoreMR(3, (L4_Word_t *) &ias);
	
	if (ias.raw == 0)
	{
	    dprintf(irq_dbg_level(irq), "INTLOGIC deliver irq immediately %d\n", irq);
	    printf("INTLOGIC immediate irq delivery %d efl %x ias %x e\n", irq, eflags, ias.raw);
	    DEBUGGER_ENTER("UNTESTED");
	    backend_sync_deliver_vector( vector, false, false, false );
	    
	    return true;
	}
    }
    
    intlogic.reraise_vector(vector, irq);
    //   L4_TBUF_RECORD_EVENT(12, "IL delay irq via window exit %d", irq);
    printf("INTLOGIC delay irq via window exit %d efl %x ias %x\n", irq, eflags, ias.raw);
    UNIMPLEMENTED();
    // inject interrupt request
    vcpu.interrupt_window_exit = true;
    
    return false;
}


static bool handle_cr_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    
    word_t gpreg = vcpu_mrs->hvm_to_gpreg(qual.mov_cr.mov_cr_gpr);
    word_t cr_num = qual.mov_cr.cr_num;
    
    //printf("CR fault qual %x gpreg %d\n", qual.raw, gpreg);
    
    switch (qual.mov_cr.access_type)
    {
    case hvm_vmx_ei_qual_t::to_cr:
	printf("mov %s (%x)->cr%d\n", vcpu_mrs->regname(gpreg), 
	       vcpu_mrs->gpregs_item.regs.reg[gpreg], cr_num);
	switch (cr_num)
	{
	case 0:
	    get_cpu().cr0.x.raw = vcpu_mrs->gpregs_item.regs.reg[gpreg];
	    dprintf(debug_cr0_write,  "cr0 write: %x\n", get_cpu().cr0);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR0, get_cpu().cr0.x.raw);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR0_SHADOW, get_cpu().cr0.x.raw);
	    break;
	case 3:
	    get_cpu().cr3.x.raw = vcpu_mrs->gpregs_item.regs.reg[gpreg];
	    dprintf(debug_cr3_write,  "cr3 write: %x\n", get_cpu().cr3);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR3, get_cpu().cr3.x.raw);
	    break;
	case 4:
	    get_cpu().cr4.x.raw = vcpu_mrs->gpregs_item.regs.reg[gpreg];
	    dprintf(debug_cr4_write,  "cr4 write: %x\n", get_cpu().cr4);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR4, get_cpu().cr4.x.raw);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR4_SHADOW, get_cpu().cr4.x.raw);
	    break;
	default:
	    printf("unhandled mov %s->cr%d\n", vcpu_mrs->regname(gpreg), cr_num);
	    UNIMPLEMENTED();
	    break;
	}
	break;
    case hvm_vmx_ei_qual_t::from_cr:
	printf("mov cr%d->%s (%x)\n", 
	       vcpu_mrs->gpregs_item.regs.reg[gpreg], cr_num,
	       vcpu_mrs->regname(gpreg));
	switch (cr_num)
	{
	case 3:
	    vcpu_mrs->gpregs_item.regs.reg[gpreg] = get_cpu().cr3.x.raw;
	    dprintf(debug_cr_read,  "cr3 read: %x\n", get_cpu().cr3);
	    break;
	default:
	    printf("unhandled mov cr%d->%s (%x)\n", 
		   vcpu_mrs->gpregs_item.regs.reg[gpreg], cr_num, vcpu_mrs->regname(gpreg));
	    UNIMPLEMENTED();
	    break;
	}
	break;
    case hvm_vmx_ei_qual_t::clts:
    case hvm_vmx_ei_qual_t::lmsw:
	printf("unhandled clts/lmsw qual %x", qual.raw);
	UNIMPLEMENTED();
	break;
    }
    
    vcpu_mrs->load_vfault_reply();
    return true;
}


static bool handle_dr_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    
    word_t gpreg = vcpu_mrs->hvm_to_gpreg(qual.mov_dr.mov_dr_gpr);
    word_t dr_num = qual.mov_dr.dr_num;
    
    //printf("DR fault qual %x gpreg %d\n", qual.raw, gpreg);
    
    switch (qual.mov_dr.dir)
    {
    case hvm_vmx_ei_qual_t::out:
	dprintf(debug_dr, "mov %s (%x)->dr%d\n", vcpu_mrs->regname(gpreg), 
		vcpu_mrs->gpregs_item.regs.reg[gpreg], dr_num);
	get_cpu().dr[dr_num] = vcpu_mrs->gpregs_item.regs.reg[gpreg];
	break;
    case hvm_vmx_ei_qual_t::in:
	dprintf(debug_dr,"mov dr%d->%s (%x)\n", 
		vcpu_mrs->gpregs_item.regs.reg[gpreg], dr_num,
		vcpu_mrs->regname(gpreg));
	vcpu_mrs->gpregs_item.regs.reg[gpreg] = get_cpu().dr[dr_num];
	break;
    }
    vcpu_mrs->load_vfault_reply();
    return true;
}



static bool handle_cpuid()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    
    frame_t frame;
    frame.x.fields.eax = vcpu_mrs->gpregs_item.regs.eax;
    
    u32_t func = frame.x.fields.eax;
    static u32_t max_basic=0, max_extended=0;

    // Note: cpuid is a serializing instruction!

    if( max_basic == 0 )
    {
	// We need to determine the maximum inputs that this CPU permits.

	// Query for the max basic input.
	frame.x.fields.eax = 0;
	__asm__ __volatile__ ("cpuid"
    		: "=a"(frame.x.fields.eax), "=b"(frame.x.fields.ebx), 
		  "=c"(frame.x.fields.ecx), "=d"(frame.x.fields.edx)
    		: "0"(frame.x.fields.eax));
	max_basic = frame.x.fields.eax;

	// Query for the max extended input.
	frame.x.fields.eax = 0x80000000;
	__asm__ __volatile__ ("cpuid"
    		: "=a"(frame.x.fields.eax), "=b"(frame.x.fields.ebx),
		  "=c"(frame.x.fields.ecx), "=d"(frame.x.fields.edx)
    		: "0"(frame.x.fields.eax));
	max_extended = frame.x.fields.eax;

	// Restore the original request.
	frame.x.fields.eax = func;
    }

    // TODO: constrain basic functions to 3 if 
    // IA32_CR_MISC_ENABLES.BOOT_NT4 (bit 22) is set.

    // Execute the cpuid request.
    __asm__ __volatile__ ("cpuid"
	    : "=a"(frame.x.fields.eax), "=b"(frame.x.fields.ebx), 
	      "=c"(frame.x.fields.ecx), "=d"(frame.x.fields.edx)
	    : "0"(frame.x.fields.eax));

    // Start our over-ride logic.
    backend_cpuid_override( func, max_basic, max_extended, &frame );
    
    vcpu_mrs->gpregs_item.regs.eax = frame.x.fields.eax;
    vcpu_mrs->gpregs_item.regs.ecx = frame.x.fields.ecx;
    vcpu_mrs->gpregs_item.regs.edx = frame.x.fields.edx;
    vcpu_mrs->gpregs_item.regs.ebx = frame.x.fields.ebx;
    vcpu_mrs->load_vfault_reply();
    
    return true;
}

static bool handle_rdtsc()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    u64_t tsc = ia32_rdtsc();
    vcpu_mrs->gpregs_item.regs.eax = tsc;
    vcpu_mrs->gpregs_item.regs.edx = (tsc >> 32);
    vcpu_mrs->load_vfault_reply();
    return true;
}

// handle string io for various types
template <typename T>
bool string_io( word_t port, word_t count, word_t mem, bool read )
{
    T *buf = (T*)mem;
    u32_t tmp;
    for(u32_t i=0;i<count;i++) {
	if(read) 
	{
	    if(!portio_read( port, tmp, sizeof(T)*8) )
		return false;
	    *(buf++) = (T)tmp;
	}
	else 
	{
	    tmp = (u32_t) *(buf++);
	    if(!portio_write( port, tmp, sizeof(T)*8) )
	       return false;
	}
    }
    return true;
}

bool string_io_resolve_address(word_t mem, word_t pmem)
{
  
    pmem = mem;
    bool pe = get_cpu().cr0.protected_mode_enabled();
    
    if (!pe)
    {
	pmem = mem;
	return true;
    }
    
    // TODO: check for invalid selectors, base, limit
    pgent_t *pmem_ent = backend_resolve_addr( mem, pmem );
    
    if (!pmem_ent)
	return false;
    
    pmem = pmem_ent->get_address();
    return true;

}

bool handle_io_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    
    word_t port = qual.io.port_num;
    word_t bit_width = (qual.io.soa + 1) * 8;
    word_t bit_mask = ((2 << bit_width-1) - 1);
    bool dir = qual.io.dir;
    bool rep = qual.io.rep;
    bool string = qual.io.string;
    bool imm = qual.io.op_encoding;

    ASSERT(bit_width == 8 || bit_width == 16 || bit_width == 32);
    
    printf("IO fault qual %x (io %s, port %x, st %d sz %d, rep %d imm %d)\n",
	   qual.raw, (qual.io.dir == hvm_vmx_ei_qual_t::out ? "out" : "in"),
	   port, string, bit_width, rep);

    if (string)
    {
	word_t mem = vcpu_mrs->hvm.ai_info;
	word_t pmem;
	word_t count = rep ? count = vcpu_mrs->gpregs_item.regs.ecx & bit_mask : 1;

	dprintf(debug_portio, "string %s port %x mem %x (p %x)\n", 
		(dir ? "read" : "write"), port, mem, pmem);
	DEBUGGER_ENTER("UNTESTED");
 
	    
	if ((mem + (count * bit_width / 8)) > ((mem + 4096) & PAGE_MASK))
	{
	    printf("unimplemented string IO across page boundaries");
	    UNIMPLEMENTED();
	}
	
	if (!string_io_resolve_address(mem, pmem))
	{
	    printf("string IO %x mem %x pfault\n", port, mem);
	    DEBUGGER_ENTER("UNTESTED");
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR2, mem);
	    // TODO: check if user/kernel access
	    bool user = false;
	    backend_sync_deliver_vector( 14, 0, true, (user << 2) | ((dir << 1)));
	}
	word_t error;
	switch( bit_width ) 
	{
	case 8:
	    error = string_io<u8_t>(port, count, pmem, dir);
	    break;
	case 16:
	    error = string_io<u16_t>(port, count, pmem, dir);
	    break;
	case 32:
	    error = string_io<u32_t>(port, count, pmem, dir);
	    break;
	}
	    
	if (error)
	{
	    dprintf(debug_portio_unhandled, "unhandled string IO %x mem %x (p %x)\n", 
		    port, mem, pmem);
	    // TODO inject exception?
	}
	if (dir)
	    vcpu_mrs->gpregs_item.regs.esi += count * bit_width / 8;
	else
	    vcpu_mrs->gpregs_item.regs.edi += count * bit_width / 8;
	    
    }
    else
    {
	if (dir)
	{
	    word_t reg = 0;
	    
	    if( !portio_read(port, reg, bit_width) ) 
	    {
		dprintf(debug_portio_unhandled, "Unhandled port read, port %x val %x IP %x\n", 
			port, reg, vcpu_mrs->gpregs_item.regs.eip);
		
		// TODO inject exception?
	    }
	    vcpu_mrs->gpregs_item.regs.eax &= ~bit_mask;
	    vcpu_mrs->gpregs_item.regs.eax |= (reg & bit_mask);
	    dprintf(debug_portio, "read port %x val %x\n", port, reg);
	}
	else
	{
	    word_t reg = vcpu_mrs->gpregs_item.regs.eax & bit_mask;
	    dprintf(debug_portio, "write port %x val %x\n", port, reg);
	    
	    if( !portio_write( port, reg, bit_width) ) 
	    {
		dprintf(debug_portio_unhandled, "Unhandled port write, port %x val %x IP %x\n", 
			port, reg, vcpu_mrs->gpregs_item.regs.eip);
		// TODO inject exception?
	    }
	}
    }
    
    vcpu_mrs->load_vfault_reply();
    return true;
}

bool backend_handle_vfault_message()
{
   
    vcpu_t &vcpu = get_vcpu();
    mr_save_t *vcpu_mrs = &vcpu.main_info.mr_save;
    
    word_t reason = vcpu_mrs->get_hvm_reason();
    dprintf(debug_hvm_fault, "hvm fault %d\n", reason);

    switch (reason) 
    {
    case hvm_vmx_reason_cr:
	return handle_cr_fault();
	break;
    case hvm_vmx_reason_dr:
	return handle_dr_fault();
	UNIMPLEMENTED();	     
	break;
    case hvm_vmx_reason_tasksw:
	printf("Taskswitch fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;
    case hvm_vmx_reason_cpuid:
	return handle_cpuid();
	break;
    case hvm_vmx_reason_hlt:
    case hvm_vmx_reason_invd:
    case hvm_vmx_reason_invlpg:
    case hvm_vmx_reason_rdpmc:
    case hvm_vmx_reason_rdtsc:
	return handle_rdtsc();
	break;
    case hvm_vmx_reason_rsm:
	printf("hlt/invd/invlpg/... fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;	
    case hvm_vmx_reason_vmcall:
    case hvm_vmx_reason_vmclear:
    case hvm_vmx_reason_vmlaunch:
    case hvm_vmx_reason_vmptrld:
    case hvm_vmx_reason_vmptrst:
    case hvm_vmx_reason_vmread:
    case hvm_vmx_reason_vmresume:
    case hvm_vmx_reason_vmwrite:
    case hvm_vmx_reason_vmxoff:
    case hvm_vmx_reason_vmxon:
	printf("VM instruction fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;
    case hvm_vmx_reason_exp_nmi:
	printf("VM exception/nmi fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;
    case hvm_vmx_reason_io:
	return handle_io_fault();
	break;
    case hvm_vmx_reason_rdmsr:        
    case hvm_vmx_reason_wrmsr:
	printf("VM MSR fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;
	//return handle_msr_write();
	//return handle_msr_read();
    case hvm_vmx_reason_iw:	
	vcpu.interrupt_window_exit = false;
	printf("VM iw exit qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	DEBUGGER_ENTER("UNIMPLEMENTED delayed fault");
	break;
    default:
	printf("VM unhandled fault %d qual %x ilen %d\n", 
	       reason, vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	DEBUGGER_ENTER("unhandled vfault message");
	return false;
    }
    return false;
}


pgent_t *
backend_resolve_addr( word_t user_vaddr, word_t &kernel_vaddr )
{
    DEBUGGER_ENTER("UNTESTED RESOLV ADDR");
    
    ASSERT(get_cpu().cr0.protected_mode_enabled());
	
    pgent_t *pdir = (pgent_t*)(get_cpu().cr3.get_pdir_addr());
    pdir += pgent_t::get_pdir_idx(user_vaddr);

    if(!pdir->is_valid()) {
	printf( "Invalid pdir entry\n");
	return 0;
    }
    if(pdir->is_superpage()) 
	return pdir;

    pgent_t *ptab = (pgent_t*)pdir->get_address();
    ptab += pgent_t::get_ptab_idx(user_vaddr);

    if(!ptab->is_valid()) {
	printf( "Invalid ptab entry\n");
	return 0;
    }
    return ptab;

}

bool vcpu_t::handle_wedge_pfault(thread_info_t *ti, map_info_t &map_info, bool &nilmapping)
{
    const word_t wedge_paddr = get_wedge_paddr();
    const word_t wedge_end_paddr = get_wedge_end_paddr();
    vcpu_t &vcpu = get_vcpu();
    
    if (ti->get_tid() == vcpu.irq_gtid || ti->get_tid() == vcpu.irq_ltid)
    {
	idl4_fpage_t fp;
	CORBA_Environment ipc_env = idl4_default_environment;

	dprintf(debug_pfault, "Wedge pfault from IRQ thread %x\n", map_info.addr);
	idl4_set_rcv_window( &ipc_env, L4_CompleteAddressSpace );
	IResourcemon_pagefault( L4_Pager(), map_info.addr, map_info.addr, map_info.rwx, &fp, &ipc_env);
	nilmapping = true;
	return true;
    }	
    
    if ((map_info.addr >= wedge_paddr) && (map_info.addr < wedge_end_paddr))
    {
	printf( "Wedge (Phys) pfault, subsitute with scratch page %x\n", get_vm()->wedge_gphys);
	map_info.addr = get_vm()->wedge_gphys;
	return true;
    }
    return false;
	
}

bool vcpu_t::resolve_paddr(thread_info_t *ti, map_info_t &map_info, word_t &paddr, bool &nilmapping)
{
    paddr = map_info.addr;
    return false;
}


bool vm_t::init(word_t ip, word_t sp)
{
    
    // find first elf file among the modules, assume that it is the kernel
    // find first non elf file among the modules assume that it is a ramdisk
    guest_kernel_module = NULL;
    ramdisk_start = NULL;
    ramdisk_size = 0;
    entry_ip = ip;
    entry_sp = sp;
    
    module_manager_t *mm = get_module_manager();
    ASSERT( mm );
    
    for( L4_Word_t idx = 0; idx < mm->get_module_count(); ++idx )
    {
	module_t *module = mm->get_module( idx );
	ASSERT( module );
	
	bool valid_elf = elf_is_valid( module->vm_offset );
	if( guest_kernel_module == NULL && valid_elf == true )
	{
	    guest_kernel_module = module;
	}
	else if( ramdisk_size == 0 && valid_elf == false )
	{
	    ramdisk_start = module->vm_offset;
	    ramdisk_size = module->size;
	}
	
	if( guest_kernel_module != NULL && ramdisk_size > 0 )
	{
	    break;
	}
    }
    
    if( ramdisk_size > 0 ) {
	printf( "Found ramdisk at %x size %d\n", ramdisk_start, ramdisk_size);	    
	resourcemon_shared.ramdisk_start = ramdisk_start;
	resourcemon_shared.ramdisk_size = ramdisk_size;
    }
    else 
    {
	printf( "No ramdisk found.\n");
	return false;
    }

    return true;
}

bool vm_t::load_elf( L4_Word_t elf_start )
{
    word_t elf_base_addr, elf_end_addr;
    elf_ehdr_t *ehdr;
    word_t kernel_vaddr_offset;
    
    if( NULL == ( ehdr = elf_is_valid( elf_start ) ) )
    {
	printf( "Not a valid elf binary.\n");
	return false;
    }
    
    // install the binary in the first 64MB
    // convert to paddr
    if( ehdr->entry >= MB(64) )
	entry_ip = (MB(64)-1) & ehdr->entry;
    else
	entry_ip = ehdr->entry;
    
    // infer the guest's vaddr offset from its ELF entry address
    kernel_vaddr_offset = ehdr->entry - entry_ip;
    	    
    if( !ehdr->load_phys( kernel_vaddr_offset ) )
    {
	printf( "Elf loading failed.\n");
	return false;
    }

    ehdr->get_phys_max_min( kernel_vaddr_offset, elf_end_addr, elf_base_addr );

    dprintf(debug_startup, "ELF Binary is residing at guest address [%x-%x] with entry point %x\n",
	    elf_base_addr, elf_end_addr, entry_ip);

    return true;
}



bool vm_t::init_guest( void )
{
    // check requested guest phys mem size
    if( guest_kernel_module != NULL )
    {
	gphys_size = guest_kernel_module->get_module_param_size( "physmem=" );
    }
    
    if( gphys_size == 0 )
    {
	gphys_size = resourcemon_shared.phys_offset;
    } 
   
    // round to 4MB 
    gphys_size &= ~(MB(4)-1);
    
    // Subtract 4MB as scratch memory
    gphys_size -= MB(4);
    wedge_gphys = gphys_size;
    
    dprintf(debug_startup, "%dM of guest phys mem available.\n", (gphys_size >> 20));

#if 0 
    if( gphys_size > (word_t) &afterburn_c_runtime_init )
    {
	printf( "Make sure that the wedge is installed above the maximum guest physical address.\n");
	return false;
    }
#endif
     
    ASSERT( gphys_size > 0 );

    // Copy rombios and vgabios to their dedicated locations
    memmove( (void*)(0xf0000), _binary_rombios_bin_start, _binary_rombios_bin_end - _binary_rombios_bin_start);
    memmove( (void*)(0xc0000), _binary_vgabios_bin_start, _binary_vgabios_bin_end - _binary_vgabios_bin_start);


    // move ramdsk to guest phys space if not already there,
    // or out of guest phys space if we are using it as a real disk
    if( ramdisk_size > 0 ) 
    {
	if( guest_kernel_module == NULL || ramdisk_start + ramdisk_size >= gphys_size ) 
	{
	    L4_Word_t newaddr = gphys_size - ramdisk_size - 1;
	    
	    // align
	    newaddr &= PAGE_MASK;
	    ASSERT( newaddr + ramdisk_size < gphys_size );
	    
	    // move
	    memmove( (void*) newaddr, (void*) ramdisk_start, ramdisk_size );
	    ramdisk_start = newaddr;
	    resourcemon_shared.ramdisk_start = newaddr;
	    resourcemon_shared.ramdisk_size = ramdisk_size;
	}
	if( guest_kernel_module == NULL ) 
	{
	    gphys_size = ramdisk_start;
	    printf( "Reducing guest phys mem to %dM for RAM disk\n", gphys_size >> 20);
	}
    }
    
    // load the guest kernel module
    if( guest_kernel_module == NULL ) 
    {
	if( ramdisk_size < 512 )
	{
	    printf( "No guest kernel module or RAM disk.\n");
	    // set BIOS POST entry point
	    entry_ip = 0xe05b;
	    entry_sp = 0x0000;
	    entry_cs = 0xf000;
	    entry_ss = 0x0000;
	}
	else 
	{
	    // load the boot sector to the fixed location of 0x7c00
	    entry_ip = 0x7c00;
	    entry_sp = 0x0000;
	    entry_cs = 0x0000;
	    entry_ss = 0x0000;
	    // workaround for a bug causing the first byte to be overwritten,
	    // probably in resource monitor
	    *((u8_t*) ramdisk_start) = 0xeb;
	    memmove( 0x0000, (void*) ramdisk_start, ramdisk_size );
	}

    } 
    else 
    {
	if( !load_elf( guest_kernel_module->vm_offset ) )
	{
	    printf( "Loading the guest kernel module failed.\n");
	    return false;
	}

	// save cmdline
	// TODO: really save it in case module gets overwritten somehow
	cmdline = (char*) guest_kernel_module->cmdline_options();
    }
    
    return true;
}



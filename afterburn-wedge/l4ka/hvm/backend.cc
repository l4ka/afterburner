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
#include INC_ARCH(hvm_vmx.h)
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(module_manager.h)

// 4MB scratch space to subsitute with wedge memory
L4_Word_t dummy_wedge_page;

extern word_t afterburn_c_runtime_init;
extern unsigned char _binary_rombios_bin_start[];
extern unsigned char _binary_rombios_bin_end[];
extern unsigned char _binary_vgabios_bin_start[];
extern unsigned char _binary_vgabios_bin_end[];


extern bool vm8086_interrupt_emulation(word_t vector, bool hw);

bool backend_load_vcpu(vcpu_t &vcpu )
{
    module_manager_t *mm = get_module_manager();
    L4_Word_t ramdisk_start = 0;
    L4_Word_t ramdisk_size = 0;
    module_t *kernel = NULL;
    
    ASSERT( mm );
    
    for( L4_Word_t idx = 0; idx < mm->get_module_count(); ++idx )
    {
	module_t *module = mm->get_module( idx );
	ASSERT( module );
	
	bool valid_elf = elf_is_valid( module->vm_offset );
	if( kernel == NULL && valid_elf == true )
	    kernel = module;
	else if( ramdisk_size == 0 && valid_elf == false )
	{
	    ramdisk_start = module->vm_offset;
	    ramdisk_size = module->size;
	    resourcemon_shared.ramdisk_start = ramdisk_start;
	    resourcemon_shared.ramdisk_size = ramdisk_size;
	}	
	
	if( kernel && ramdisk_size )
	    break;
    }

    if( kernel  ) 
	printf( "Found guest kernel at %x size %d\n", kernel->vm_offset);	    
    else
	printf( "No guest kernel found\n");	    
	
    if( ramdisk_size ) 
	printf( "Found ramdisk at %x size %d\n", ramdisk_start, ramdisk_size);	    
    else 
	printf( "No ramdisk found.\n");
    
    // check requested guest phys mem size
    // round to 4MB 
    resourcemon_shared.phys_size &= ~(MB(4)-1);
    
    // Subtract 4MB as scratch memory
    resourcemon_shared.phys_size -= MB(4);
    dummy_wedge_page = resourcemon_shared.phys_size;
    ASSERT( resourcemon_shared.phys_size > 0 );
    
    dprintf(debug_startup, "%dM of guest phys mem available.\n", (resourcemon_shared.phys_size >> 20));

    // Copy rombios and vgabios to their dedicated locations
    memmove( (void*)(0xf0000), _binary_rombios_bin_start, _binary_rombios_bin_end - _binary_rombios_bin_start);
    memmove( (void*)(0xc0000), _binary_vgabios_bin_start, _binary_vgabios_bin_end - _binary_vgabios_bin_start);


    // move ramdsk to guest phys space if not already there,
    // or out of guest phys space if we are using it as a real disk
    if( ramdisk_size > 0 ) 
    {
	if( kernel == NULL || ramdisk_start + ramdisk_size >= resourcemon_shared.phys_size ) 
	{
	    L4_Word_t newaddr = resourcemon_shared.phys_size - ramdisk_size - 1;
	    
	    // align
	    newaddr &= PAGE_MASK;
	    ASSERT( newaddr + ramdisk_size < resourcemon_shared.phys_size );
	    
	    // move
	    memmove( (void*) newaddr, (void*) ramdisk_start, ramdisk_size );
	    ramdisk_start = newaddr;
	    resourcemon_shared.ramdisk_start = newaddr;
	    resourcemon_shared.ramdisk_size = ramdisk_size;
	}
	
	if( kernel == NULL ) 
	{
	    resourcemon_shared.phys_size = ramdisk_start;
	    printf( "Reducing guest phys mem to %dM for RAM disk\n", resourcemon_shared.phys_size >> 20);
	}
    }
    
    // load the guest kernel module
    if( kernel == NULL ) 
    {
	vcpu.init_info.real_mode = true;
	if( ramdisk_size < 512 )
	{
	    printf( "No guest kernel module or RAM disk.\n");
	    // set BIOS POST entry point
	    vcpu.init_info.entry_ip = 0xe05b;
	    vcpu.init_info.entry_sp = 0x0000;
	    vcpu.init_info.entry_cs = 0xf000;
	    vcpu.init_info.entry_ss = 0x0000;
	}
	else 
	{
	    // load the boot sector to the fixed location of 0x7c00
	    vcpu.init_info.entry_ip = 0x7c00;
	    vcpu.init_info.entry_sp = 0x0000;
	    vcpu.init_info.entry_cs = 0x0000;
	    vcpu.init_info.entry_ss = 0x0000;
	    // workaround for a bug causing the first byte to be overwritten,
	    // probably in resource monitor
	    *((u8_t*) ramdisk_start) = 0xeb;
	    memmove( 0x0000, (void*) ramdisk_start, ramdisk_size );
	}

    } 
    else 
    {
	vcpu.init_info.real_mode = false;
	word_t elf_start = kernel->vm_offset;
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
	    vcpu.init_info.entry_ip = (MB(64)-1) & ehdr->entry;
	else
	    vcpu.init_info.entry_ip = ehdr->entry;
	
	// infer the guest's vaddr offset from its ELF entry address
	kernel_vaddr_offset = ehdr->entry - vcpu.init_info.entry_ip;
	
	if( !ehdr->load_phys( kernel_vaddr_offset ) )
	{
	    printf( "Elf loading failed.\n");
	    return false;
	}
	
	ehdr->get_phys_max_min( kernel_vaddr_offset, elf_end_addr, elf_base_addr );
	
	dprintf(debug_startup, "ELF Binary at guest address [%x-%x] with entry point %x\n",
		elf_base_addr, elf_end_addr, vcpu.init_info.entry_ip);
	
	// save cmdline
	vcpu.init_info.cmdline = (char*) kernel->cmdline_options();
    }
    return true;
}

bool backend_sync_deliver_exception( exc_info_t exc, L4_Word_t error_code )
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    if( get_cpu().cr0.real_mode() ) 
    {
	// Real mode, emulate interrupt injection
	return vm8086_interrupt_emulation(exc.vector, true);
    }
    
    vcpu_mrs->append_exc_item(exc.raw, error_code, vcpu_mrs->hvm.ilen);
    
    debug_id_t id = ((exc.hvm.type == hvm_vmx_int_t::ext_int) ? debug_irq : debug_exception);
    dprintf(id, "deliver exception %x (t %d vec %d eecv %c), eec %d, ilen %d\n", 
	    exc.raw, exc.hvm.type, exc.hvm.vector, exc.hvm.err_code_valid ? 'y' : 'n', 
	    error_code, vcpu_mrs->exc_item.regs.entry_ilen);
		   
    return true;
}


// signal to vcpu that we would like to deliver an interrupt
bool backend_async_deliver_irq( intlogic_t &intlogic )
{
    word_t vector, irq;
    
    vcpu_t vcpu = get_vcpu();
    cpu_t & cpu = get_cpu();

    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    L4_Word_t mr = 1;
    
    // are we already waiting for an interrupt window exit
    hvm_vmx_exectr_cpubased_t cpubased;
    cpubased.raw = vcpu_mrs->execctrl_item.regs.cpu;

    if( cpubased.iw )
	return false;
    
    if( !intlogic.pending_vector( vector, irq ) )
	return false;

    
    //Asynchronously read eflags and ias
    
    vcpu_mrs->init_msg();
    vcpu_mrs->append_gpr_item(L4_CTRLXFER_GPREGS_EFLAGS, 0, true);
    vcpu_mrs->append_nonreg_item(L4_CTRLXFER_NONREGS_INT, 0, true);
    vcpu_mrs->append_exc_item(0, 0, 0);
    vcpu_mrs->load_gpr_item();
    vcpu_mrs->load_exc_item();
    vcpu_mrs->load_nonreg_item();
    L4_ReadCtrlXferItems(vcpu.main_gtid);
    vcpu_mrs->store_gpr_item(mr);
    vcpu_mrs->store_exc_item(mr);
    vcpu_mrs->store_nonreg_item(mr);
    hvm_vmx_gs_ias_t ias;
    hvm_vmx_int_t exc_info;
    
    ias.raw = vcpu_mrs->nonreg_item.regs.ias;
    exc_info.raw = vcpu_mrs->exc_item.regs.entry_info;
    cpu.flags.x.raw = vcpu_mrs->gpr_item.regs.eflags;

    // do we already have a pending exception
    if (exc_info.valid)
    {
	dprintf(irq_dbg_level(irq)+1,
		"INTLOGIC async already pending irq %d vec %d efl %x (%c) ias %x\n", irq, 
		vector, get_cpu().flags, (get_cpu().interrupts_enabled() ? 'I' : 'i'), 
		ias.raw);
	intlogic.reraise_vector(vector);
	return false;
    }

    if (cpu.interrupts_enabled() && ias.raw == 0)
    {
	dprintf(irq_dbg_level(irq)+1,
		"INTLOGIC async irq %d vector %d efl %x (%c) ias %x\n", irq, 
		vector, cpu.flags, (cpu.interrupts_enabled() ? 'I' : 'i'), 
		ias.raw);
	
	exc_info_t exc;
	exc.raw = 0;
	exc.hvm.type = hvm_vmx_int_t::ext_int;
	exc.hvm.vector = vector;
	exc.hvm.err_code_valid = 0;
	exc.hvm.valid = 1;
	
	vcpu_mrs->append_exc_item(exc.raw, 0, 0);
	L4_Set_MsgTag (L4_Niltag);
	vcpu_mrs->load_exc_item();
	L4_WriteCtrlXferItems(vcpu.main_gtid);
	return true;
    }
    
    intlogic.reraise_vector(vector);
    
    dprintf(irq_dbg_level(irq)+1, 
	    "INTLOGIC async iwe irq %d vec %d  efl %x (%c) ias %x\n", 
	    irq, vector, get_cpu().flags, (get_cpu().interrupts_enabled() ? 'I' : 'i'), ias.raw);
    
    // inject interrupt request via exregs
    cpubased.iw = 1;
    L4_Set_MsgTag (L4_Niltag);
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_CPU, cpubased.raw);
    vcpu_mrs->load_execctrl_item();
    L4_WriteCtrlXferItems(vcpu.main_gtid);
    return false;
}


pgent_t *
backend_resolve_addr( word_t user_vaddr, word_t &kernel_vaddr )
{
    ASSERT(get_cpu().cr0.protected_mode_enabled());
    ASSERT(get_cpu().cr4.is_pse_enabled());

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


static bool handle_cr_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    
    word_t gpreg = vcpu_mrs->hvm_to_gpreg(qual.mov_cr.mov_cr_gpr);
    word_t cr_num = qual.mov_cr.cr_num;
    
    dprintf(debug_hvm_fault, "CR fault qual %x gpreg %d\n", qual.raw, gpreg);
    
    switch (qual.mov_cr.access_type)
    {
    case hvm_vmx_ei_qual_t::to_cr:
	dprintf(debug_hvm_fault,"mov %C (%08x)->cr%d\n", vcpu_mrs->regnameword(gpreg), 
		vcpu_mrs->gpr_item.regs.reg[gpreg], cr_num);
	switch (cr_num)
	{
	case 0:
	    if ((get_cpu().cr0.x.raw ^ vcpu_mrs->gpr_item.regs.reg[gpreg]) & X86_CR0_PE)
	    {
		if (vcpu_mrs->gpr_item.regs.reg[gpreg] & X86_CR0_PE)
		{
		    printf("switch to protected mode");
		    DEBUGGER_ENTER("UNIMPLEMENTED");
		    // Get CS,. ... 
		}
		else
		{
		    printf("untested switch to real mode");
		    DEBUGGER_ENTER("UNIMPLEMENTED");				
		    // Get CS,. ... 
		}
	    }
	    get_cpu().cr0.x.raw = vcpu_mrs->gpr_item.regs.reg[gpreg];
	    dprintf(debug_cr0_write,  "cr0 write: %x\n", get_cpu().cr0);
	    
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR0, get_cpu().cr0.x.raw);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR0_SHADOW, get_cpu().cr0.x.raw);
	    break;
	case 3:
	    get_cpu().cr3.x.raw = vcpu_mrs->gpr_item.regs.reg[gpreg];
	    dprintf(debug_cr3_write,  "cr3 write: %x\n", get_cpu().cr3);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR3, get_cpu().cr3.x.raw);
	    break;
	case 4:
	    get_cpu().cr4.x.raw = vcpu_mrs->gpr_item.regs.reg[gpreg];
	    dprintf(debug_cr4_write,  "cr4 write: %x\n", get_cpu().cr4);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR4, get_cpu().cr4.x.raw);
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR4_SHADOW, get_cpu().cr4.x.raw);
	    break;
	default:
	    printf("unhandled mov %C->cr%d\n", vcpu_mrs->regnameword(gpreg), cr_num);
	    UNIMPLEMENTED();
	    break;
	}
	break;
    case hvm_vmx_ei_qual_t::from_cr:
	switch (cr_num)
	{
	case 3:
	    vcpu_mrs->gpr_item.regs.reg[gpreg] = get_cpu().cr3.x.raw;
	    dprintf(debug_cr_read,  "cr3 read: %x\n", get_cpu().cr3);
	    break;
	default:
	    printf("unhandled mov cr%d->%C (%08x)\n", 
		   vcpu_mrs->gpr_item.regs.reg[gpreg], cr_num, vcpu_mrs->regnameword(gpreg));
	    UNIMPLEMENTED();
	    break;
	}
	dprintf(debug_hvm_fault, "mov cr%d->%C (%08x)\n", 
		cr_num, vcpu_mrs->regnameword(gpreg), vcpu_mrs->gpr_item.regs.reg[gpreg]);
	break;
    case hvm_vmx_ei_qual_t::clts:
    case hvm_vmx_ei_qual_t::lmsw:
	printf("unhandled clts/lmsw qual %x", qual.raw);
	UNIMPLEMENTED();
	break;
    }
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}


static bool handle_dr_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    
    word_t gpreg = vcpu_mrs->hvm_to_gpreg(qual.mov_dr.mov_dr_gpr);
    word_t dr_num = qual.mov_dr.dr_num;
    
    dprintf(debug_hvm_fault, "DR fault qual %x gpreg %d\n", qual.raw, gpreg);
    
    switch (qual.mov_dr.dir)
    {
    case hvm_vmx_ei_qual_t::out:
	dprintf(debug_hvm_fault, "mov %C (%08x)->dr%d\n", vcpu_mrs->regnameword(gpreg), 
		vcpu_mrs->gpr_item.regs.reg[gpreg], dr_num);
	get_cpu().dr[dr_num] = vcpu_mrs->gpr_item.regs.reg[gpreg];
	break;
    case hvm_vmx_ei_qual_t::in:
	vcpu_mrs->gpr_item.regs.reg[gpreg] = get_cpu().dr[dr_num];
	dprintf(debug_hvm_fault,"mov dr%d->%C (%08x)\n", 
		vcpu_mrs->gpr_item.regs.reg[gpreg], dr_num, vcpu_mrs->regnameword(gpreg));
	break;
    }
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}



static bool handle_cpuid_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    frame_t frame;
    frame.x.fields.eax = vcpu_mrs->gpr_item.regs.eax;

    dprintf(debug_hvm_fault, "CPUID fault\n");

    u32_t func = frame.x.fields.eax;
    static u32_t max_basic=0, max_extended=0;
   
    cpu_read_cpuid(&frame, max_basic, max_extended);    
    // Start our over-ride logic.
    backend_cpuid_override( func, max_basic, max_extended, &frame );

    vcpu_mrs->gpr_item.regs.eax = frame.x.fields.eax;
    vcpu_mrs->gpr_item.regs.ecx = frame.x.fields.ecx;
    vcpu_mrs->gpr_item.regs.edx = frame.x.fields.edx;
    vcpu_mrs->gpr_item.regs.ebx = frame.x.fields.ebx;
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

static bool handle_rdtsc_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    u64_t tsc = ia32_rdtsc();
    vcpu_mrs->gpr_item.regs.eax = tsc;
    vcpu_mrs->gpr_item.regs.edx = (tsc >> 32);
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

// handle string io for various types
template <typename T>
bool string_io( word_t port, word_t count, word_t mem, bool read )
{
    T *buf = (T*)mem;
    
    u32_t tmp;
    
    for(u32_t i=0;i<count;i++) 
    {
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

static bool string_io_resolve_address(word_t mem, word_t &pmem, word_t &pmem_end)
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
    
    word_t page_mask = pmem_ent->is_superpage() ? SUPERPAGE_MASK : PAGE_MASK;
    word_t page_size = pmem_ent->is_superpage() ? SUPERPAGE_SIZE : PAGE_SIZE;
    
    pmem = (pmem_ent->get_address() & page_mask) | (mem & ~page_mask);
    pmem_end = (pmem_ent->get_address() & page_mask) + page_size;
    return true;

}

static bool handle_io_fault()
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
    
    dprintf(debug_portio, "IO fault qual %x (%c, port %x, st %d sz %d, rep %d imm %d)\n",
	    qual.raw, (qual.io.dir == hvm_vmx_ei_qual_t::out ? 'o' : 'i'),
	    port, string, bit_width, rep, imm);

    if (string)
    {
	word_t mem = vcpu_mrs->hvm.ai_info;
	word_t pmem = 0, pmem_end = 0;
	word_t count = rep ? count = vcpu_mrs->gpr_item.regs.ecx & bit_mask : 1;
	word_t bytes = count * bit_width / 8;
	
	if (!string_io_resolve_address(mem, pmem, pmem_end))
	{
	    printf("string IO %x mem %x pfault\n", port, mem);
	    DEBUGGER_ENTER("UNTESTED");
	    vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR2, mem);
	    // TODO: check if user/kernel access
	    bool user = false;
	    
	    exc_info_t exc;
	    exc.raw = 0;
	    exc.hvm.type = hvm_vmx_int_t::hw_except;
	    exc.hvm.vector = 14;
	    exc.hvm.err_code_valid = true;
	    exc.hvm.valid = 1;
	    backend_sync_deliver_exception( exc, (user << 2) | ((dir << 1)));
	}
	
	if (pmem + bytes >= pmem_end)
	{
	    printf("string IO %x mem %x size %d across page boundaries [%x-%x]\n", 
		   mem, bytes, pmem, pmem_end);
	    UNIMPLEMENTED();
	}

	dprintf(debug_portio, "string %c port %x mem %x (p %x) count %d\n", 
		(dir ? 'r' : 'w'), port, mem, pmem, count);
	
	word_t error = false;
	switch( bit_width ) 
	{
	case 8:
	    error = !string_io<u8_t>(port, count, pmem, dir);
	    break;
	case 16:
	    error = !string_io<u16_t>(port, count, pmem, dir);
	    break;
	case 32:
	    error = !string_io<u32_t>(port, count, pmem, dir);
	    break;
	}
	    
	if (error)
	{
	    dprintf(debug_portio_unhandled, "unhandled string IO %x mem %x (p %x)\n", 
		    port, mem, pmem);
	    DEBUGGER_ENTER("UNTESTED");
	    // TODO inject exception?
	}
	
	if (dir)
	    vcpu_mrs->gpr_item.regs.edi += bytes;
	else
	    vcpu_mrs->gpr_item.regs.esi += bytes;

	if (rep)
	    vcpu_mrs->gpr_item.regs.ecx = 0;
		
	
    }
    else
    {
	if (dir)
	{
	    word_t reg = 0;
	    
	    if( !portio_read(port, reg, bit_width) ) 
	    {
		dprintf(debug_portio_unhandled, "Unhandled port read, port %x val %x IP %x\n", 
			port, reg, vcpu_mrs->gpr_item.regs.eip);
		
		// TODO inject exception?
	    }
	    vcpu_mrs->gpr_item.regs.eax &= ~bit_mask;
	    vcpu_mrs->gpr_item.regs.eax |= (reg & bit_mask);
	    dprintf(debug_portio, "read port %x val %x\n", port, reg);
	}
	else
	{
	    word_t reg = vcpu_mrs->gpr_item.regs.eax & bit_mask;
	    dprintf(debug_portio, "write port %x val %x\n", port, reg);
	    
	    if( !portio_write( port, reg, bit_width) ) 
	    {
		dprintf(debug_portio_unhandled, "Unhandled port write, port %x val %x IP %x\n", 
			port, reg, vcpu_mrs->gpr_item.regs.eip);
		// TODO inject exception?
	    }
	}
    }
    
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

static bool handle_exp_nmi(exc_info_t exc, word_t eec, word_t cr2)
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    ASSERT(exc.hvm.valid == 1);

    if( !get_cpu().cr0.protected_mode_enabled())
    {
	printf("unimplemented real mode exp/nmi");
	UNIMPLEMENTED();
    }
    
    if (exc.hvm.type == hvm_vmx_int_t::sw_except)
	dprintf(debug_hvm_fault, 
	    "hvm swexc %d with idt evt %x (type %d vec %d eecv %c), eec %d ip %x ilen %d\n", 
		exc.raw, exc.hvm.type, exc.hvm.vector, exc.hvm.err_code_valid ? 'y' : 'n', 
		vcpu_mrs->exc_item.regs.idt_eec, vcpu_mrs->gpr_item.regs.eip, vcpu_mrs->hvm.ilen);

    
    switch (exc.vector)
    {
    case X86_EXC_DEBUG:
	UNIMPLEMENTED();
	break;
    case X86_EXC_PAGEFAULT:
	get_cpu().cr2 = cr2;
	vcpu_mrs->append_cr_item(L4_CTRLXFER_CREGS_CR2, cr2);
	break;
    case X86_EXC_GENERAL_PROTECTION:
	break;
    default: 
	break;
    }
    
    /* Inject exception */
    backend_sync_deliver_exception(exc, eec);
    return true;
}


static bool handle_exp_nmi_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    exc_info_t exc;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    exc.raw = vcpu_mrs->exc_item.regs.exit_info;
    
    dprintf(debug_exception, "exp/nmi fault %x (type %d vec %d eecv %c), eec %d, qual %x, ilen %d\n", 
	    exc.raw, exc.hvm.type, exc.hvm.vector, exc.hvm.err_code_valid ? 'y' : 'n', 
	    vcpu_mrs->exc_item.regs.exit_eec, qual.raw, vcpu_mrs->hvm.ilen);

    return handle_exp_nmi(exc, vcpu_mrs->exc_item.regs.exit_eec, qual.raw);
}

static bool handle_iw()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;

    // disable IW exit
    hvm_vmx_exectr_cpubased_t cpubased;
    cpubased.raw = vcpu_mrs->execctrl_item.regs.cpu;
    ASSERT(cpubased.iw == 1);
    cpubased.iw = 0;
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_CPU, cpubased.raw);

    word_t vector, irq;
    UNUSED bool pending = get_intlogic().pending_vector(vector, irq);
    ASSERT(pending);

    dprintf(irq_dbg_level(irq)+1, "irq %d vector %d iw fault qual %x ilen %d\n", 
	    irq, vector, vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);

    //Inject IRQ
    exc_info_t exc;
    exc.raw = 0;
    exc.hvm.type = hvm_vmx_int_t::ext_int;
    exc.hvm.vector = vector;
    exc.hvm.err_code_valid = 0;
    exc.hvm.valid = 1;
    backend_sync_deliver_exception(exc, 0);
    return true;
}


static bool handle_rdmsr_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    
    
    word_t msr_num = vcpu_mrs->gpr_item.regs.ecx;
    dprintf(debug_hvm_fault, "RDMSR fault msr %x\n", msr_num);
    
    // Use otherreg item in mrs as cache for sysenter MSRs
    switch(msr_num)
    {
    case 0x174: // sysenter_cs
	dprintf(debug_msr, "RDMSR sysenter CS %x val %x \n", msr_num, 
		vcpu_mrs->otherreg_item.regs.sysenter_cs);
	vcpu_mrs->gpr_item.regs.eax = vcpu_mrs->otherreg_item.regs.sysenter_cs;
	vcpu_mrs->gpr_item.regs.edx = 0;
	break;
    case 0x175: // sysenter_esp
	dprintf(debug_msr, "RDMSR sysenter ESP %x val %x \n", msr_num, 
		vcpu_mrs->otherreg_item.regs.sysenter_esp);
	vcpu_mrs->gpr_item.regs.eax = vcpu_mrs->otherreg_item.regs.sysenter_eip;
	vcpu_mrs->gpr_item.regs.edx = 0;
	break;
    case 0x176: // sysenter_eip
	dprintf(debug_msr, "RDMSR sysenter EIP %x val %x \n", msr_num, 
		vcpu_mrs->otherreg_item.regs.sysenter_eip);
	vcpu_mrs->gpr_item.regs.eax = vcpu_mrs->otherreg_item.regs.sysenter_eip;
	vcpu_mrs->gpr_item.regs.edx = 0;
	break;
    default:
	dprintf(debug_msr, "unhandled RDMSR %x\n", msr_num);
	vcpu_mrs->gpr_item.regs.eax = 0;
	vcpu_mrs->gpr_item.regs.edx = 0;
	break;
    }

    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

static bool handle_wrmsr_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    word_t msr_num = vcpu_mrs->gpr_item.regs.ecx;
    u64_t value = vcpu_mrs->gpr_item.regs.edx;
    
    value <<= 32;
    value |= vcpu_mrs->gpr_item.regs.eax;
    
    dprintf(debug_hvm_fault, "WRMSR fault msr %x val <%x:%x>\n", msr_num,
	    vcpu_mrs->gpr_item.regs.edx,vcpu_mrs->gpr_item.regs.eax); 
    
    // Use otherreg item in mrs as cache for sysenter MSRs
    switch(msr_num)
    {
    case 0x174: // sysenter_cs
	dprintf(debug_msr, "WRMSR sysenter CS %x val %x \n", msr_num, 
		vcpu_mrs->gpr_item.regs.eax);
	vcpu_mrs->append_otherreg_item(L4_CTRLXFER_OTHERREGS_SYS_CS, 
				       vcpu_mrs->gpr_item.regs.eax);
	break;
    case 0x175: // sysenter_esp
	dprintf(debug_msr, "WRMSR sysenter ESP %x val %x \n", msr_num, 
		vcpu_mrs->gpr_item.regs.eax);
	vcpu_mrs->append_otherreg_item(L4_CTRLXFER_OTHERREGS_SYS_ESP, 
				       vcpu_mrs->gpr_item.regs.eax);
	break;
    case 0x176: // sysenter_eip
	dprintf(debug_msr, "WRMSR sysenter EIP %x val %x \n", msr_num, 
		vcpu_mrs->gpr_item.regs.eax);
	vcpu_mrs->append_otherreg_item(L4_CTRLXFER_OTHERREGS_SYS_EIP, 
				       vcpu_mrs->gpr_item.regs.eax);
	break;
    default:
	dprintf(debug_msr, "unhandled WRMSR fault msr %x val <%x:%x>\n", msr_num,
		vcpu_mrs->gpr_item.regs.edx, vcpu_mrs->gpr_item.regs.eax); 
	break;
    }
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

static bool handle_invlpg_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    dprintf(debug_flush, "INVLPG fault %x\n", vcpu_mrs->hvm.qual);
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

static bool handle_invd_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    dprintf(debug_flush, "INVD fault\n");
    
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    return true;
}

static bool handle_vm_instruction()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    printf("unhandled VM instruction fault qual %x ilen %d\n", 
	   vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
    vcpu_mrs->dump(debug_id_t(0,0), true);
    UNIMPLEMENTED();	     
    return false;
}

static bool handle_hlt_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    dprintf(debug_idle, "HLT fault\n");
    
    //Check if guest is blocked by sti (sti;hlt) and unblock
    hvm_vmx_gs_ias_t ias;
    ias.raw = vcpu_mrs->nonreg_item.regs.ias;

    if (ias.bl_sti == 1)
    {
	dprintf(debug_idle, "set ias to non-blocked by sti after HLT\n");
	ias.bl_sti = 0;
	vcpu_mrs->append_nonreg_item(L4_CTRLXFER_NONREGS_INT, ias.raw);
    }
    
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    //Just return without a reply, IRQ thread will send a
    // wakeup message at the next IRQ
    return false;

}

static bool handle_pause_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    /* Disable pause exiting for now */
    hvm_vmx_exectr_cpubased_t cpubased;
    cpubased.raw = vcpu_mrs->execctrl_item.regs.cpu;

    cpubased.pause = false;
    vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_CPU, cpubased.raw);
    return true;
}

static bool handle_monitor_fault()
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    vcpu_mrs->gpr_item.regs.eip += vcpu_mrs->hvm.ilen;
    DEBUGGER_ENTER("hvm monitor catcher");
    /* Disable monitor/mwait exiting for now */
    //hvm_vmx_exectr_cpubased_t cpubased;
    //cpubased.raw = vcpu_mrs->execctrl_item.regs.cpu;

    //cpubased.monitor = false;
    //vcpu_mrs->append_execctrl_item(L4_CTRLXFER_EXEC_CPU, cpubased.raw);
    return true;
}

static bool handle_idt_evt(word_t reason)
{
    mr_save_t *vcpu_mrs = &get_vcpu().main_info.mr_save;
    exc_info_t exc;
    exc.raw = vcpu_mrs->exc_item.regs.idt_info;
    hvm_vmx_ei_qual_t qual;
    qual.raw = vcpu_mrs->hvm.qual;
    cpu_t &cpu = get_cpu();
    
    dprintf(debug_hvm_fault, 
	    "hvm fault %d with idt evt %x (type %d vec %d eecv %c), eec %d cr2 %x ip %x\n", 
	    reason, exc.raw, exc.hvm.type, exc.hvm.vector, exc.hvm.err_code_valid ? 'y' : 'n', 
	    vcpu_mrs->exc_item.regs.idt_eec, cpu.cr2, vcpu_mrs->gpr_item.regs.eip);
    
    switch (exc.hvm.type)
    {
    case hvm_vmx_int_t::ext_int:
	// Reraise external interrupt
	get_intlogic().reraise_vector(exc.hvm.vector);
	break;
    case hvm_vmx_int_t::hw_except:
 	handle_exp_nmi(exc, vcpu_mrs->exc_item.regs.idt_eec, cpu.cr2);
	break;
    default:
	handle_exp_nmi(exc, vcpu_mrs->exc_item.regs.idt_eec, cpu.cr2);
	break;
    }
    return true;
}

bool backend_handle_vfault_message()
{
    vcpu_t &vcpu = get_vcpu();
    cpu_t & cpu = get_cpu();
    mr_save_t *vcpu_mrs = &vcpu.main_info.mr_save;
    word_t vector, irq;
    word_t reason = vcpu_mrs->get_hvm_fault_reason();
    bool internal = vcpu_mrs->is_hvm_fault_internal();
    bool reply = false;
    hvm_vmx_int_t idt_info;

    dprintf(debug_hvm_fault, "hvm fault %d (%c)\n", reason, 
	    (vcpu_mrs->is_hvm_fault_internal()? 'i' : 'e'));

    // Update eflags
    cpu.flags.x.raw = vcpu_mrs->gpr_item.regs.eflags;
    
    // We may receive internal exits (i.e., those already handled by L4) because
    // of IDT event delivery during the exit
    idt_info.raw = vcpu_mrs->exc_item.regs.idt_info;
    
    if (idt_info.valid)
	reply = handle_idt_evt(reason);
    if (internal)
	goto done_irq;
    
    switch (reason) 
    {
    case hvm_vmx_reason_cr:
	reply = handle_cr_fault();
	break;
    case hvm_vmx_reason_dr:
	reply = handle_dr_fault();
	break;
    case hvm_vmx_reason_tasksw:
	printf("Taskswitch fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;
    case hvm_vmx_reason_cpuid:
	reply = handle_cpuid_fault();
	break;
    case hvm_vmx_reason_hlt:
	reply = handle_hlt_fault();
	break;	
    case hvm_vmx_reason_invd:
	reply = handle_invd_fault();
	break;
    case hvm_vmx_reason_invlpg:
	reply = handle_invlpg_fault();
	break;
    case hvm_vmx_reason_rdpmc:
	printf("rdpmc fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	UNIMPLEMENTED();	     
	break;	
    case hvm_vmx_reason_rdtsc:
	reply = handle_rdtsc_fault();
	break;
    case hvm_vmx_reason_rsm:
	printf("rsm fault qual %x ilen %d\n", vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
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
	reply = handle_vm_instruction();
	break;
    case hvm_vmx_reason_exp_nmi:
	reply = handle_exp_nmi_fault();
	break;
    case hvm_vmx_reason_io:
	reply = handle_io_fault();
	break;
    case hvm_vmx_reason_rdmsr:        
	reply = handle_rdmsr_fault();
	break;
    case hvm_vmx_reason_wrmsr:
	reply = handle_wrmsr_fault();
	break;
    case hvm_vmx_reason_iw:	
	reply = handle_iw();
	break;
    case hvm_vmx_reason_mwait:
	reply = handle_monitor_fault();
	break;
    case hvm_vmx_reason_pause:
	reply = handle_pause_fault();
	break;
    default:
	printf("VM unhandled fault %d qual %x ilen %d\n", 
	       reason, vcpu_mrs->hvm.qual, vcpu_mrs->hvm.ilen);
	DEBUGGER_ENTER("unhandled vfault message");
	reply = false;
	break;
    }
    
done_irq:
    // Check for pending irqs
    // do we already have a pending exception
    if (vcpu_mrs->exc_item.item.num_regs != 0)
	goto done;

    // are we already waiting for an interrupt window exit
    hvm_vmx_exectr_cpubased_t cpubased;
    cpubased.raw = vcpu_mrs->execctrl_item.regs.cpu;
    if( cpubased.iw )
	goto done;
    
    if( !get_intlogic().pending_vector( vector, irq ) )
	goto done;
    
    if (!vcpu_mrs->append_irq(vector))
	get_intlogic().reraise_vector(vector);
    
    reply = true;

done:
    if (reply)
	vcpu_mrs->load_vfault_reply();
    
    return reply;
}


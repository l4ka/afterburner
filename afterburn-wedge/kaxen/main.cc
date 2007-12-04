/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/main.cc
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

#include INC_ARCH(cpu.h)
#include INC_ARCH(page.h)

#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(xen_hypervisor.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(wedge.h)

#include <memory.h>
#include <elfsimple.h>
#include <burn_symbols.h>

hconsole_t con;
vcpu_t vcpu;
xen_start_info_t xen_start_info;

extern bool frontend_elf_rewrite( elf_ehdr_t *elf, word_t vaddr_offset, bool module );

static const bool debug_elf = false;
static const bool debug_ramdisk = false;
static const bool debug_start_info = false;

void xen_putc( const char c )
{
    if( xen_start_info.is_privileged_dom() )
	XEN_console_io( CONSOLEIO_write, 1, (char *)&c );
    else {
	xen_controller.console_write( c );
	if( c == '\n' )
	    xen_controller.console_write( '\r' );
    }
}

void dump_xen_start_info( void )
{
    xen_start_info_t *si = &xen_start_info;

    con << "Memory allocated to this domain: " 
	<< si->nr_pages << " pages, "
	<< (si->nr_pages * PAGE_SIZE/1024) << " KB\n";
    if( si->is_privileged_dom() )
	con << "This is a privileged domain.\n";
    if( si->is_init_dom() )
	con << "This is the initial domain.\n";
#if defined(CONFIG_XEN_2_0)
    if( si->is_blk_backend_dom() )
	con << "This is the block backend domain.\n";
    if( si->is_net_backend_dom() )
	con << "This is the net backend domain.\n";
#endif
    con << "Command line: " << (char *)si->cmd_line << '\n';

    if( !debug_start_info )
	return;

    con << "Hypervisor shared page at " << (void *)si->shared_info << '\n';
    con << "Initial page directory at " << (void *)si->pt_base << '\n';
    con << "Bootstrap page table frames: " << si->nr_pt_frames << '\n';
    con << "Page frame list at " << (void *)si->mfn_list << '\n';
    con << "Modules start at " << (void *)si->mod_start << '\n';
    con << "Modules size: " << (si->mod_len/1024) << " KB\n";
}

void dump_xen_shared_info( void )
{
    con << "Number VCPUs: " << xen_shared_info.get_num_cpus() << '\n';
    con << "CPU speed: " << (xen_shared_info.get_cpu_freq()/1000/1000) << " MHz\n";

    if( !debug_start_info )
	return;

#ifdef CONFIG_XEN_2_0
    con << "M2P table start: " 
	<< (void *)word_t(xen_shared_info.arch.mfn_to_pfn_start) << '\n';
    con << "P2M table start: " 
	<< (void *)word_t(xen_shared_info.arch.pfn_to_mfn_frame_list) << '\n';
#endif
}

static void map_shared_info( word_t ma )
{
    // NOTE: xen_shared_info is declared in the linker script.  It is
    // allocated an address, but no physical backing.  We map it to 
    // physical backing here.
    int err = XEN_update_va_mapping( word_t(&xen_shared_info), 
		(ma & PAGE_MASK) | 3, UVMF_INVLPG );
    if( err ) {
    	XEN_shutdown( SHUTDOWN_reboot );
	while( 1 );
    }
}

#if 0
static word_t map_guest_modules( word_t &ramdisk_start, word_t &ramdisk_len )
{
    word_t phys_mask = MB(64)-1; // Map the guest modules within the first 64MB.

    // Look for an ELF module.
    if( xen_start_info.mod_len == 0 )
	PANIC( "No guest OS found." );
    word_t image = xen_start_info.mod_start;
    elf_ehdr_t *ehdr = elf_is_valid( image );
    if( !ehdr )
	PANIC( "The guest OS is not a valid ELF file." );

    // Extract the physical entry point of the ELF file.
    word_t phys_entry = ehdr->entry & phys_mask;
    if( debug_elf )
	con << "ELF entry virtual address: " << (void *)ehdr->entry 
	    << " [" << (void *)phys_entry << "]\n";

    // Remap the ELF file.
    word_t end_addr = 0;
    for( word_t i = 0; i < ehdr->phnum; i++ )
    {
	// Locate the entry in the program header table.
	elf_phdr_t *ph = &ehdr->get_phdr_table()[i];

	// Is it to be loaded?
	if( ph->type == PT_LOAD )
	{
	    word_t src = image + ph->offset;
	    word_t remap_pages, dst;
	    if( ph->fsize % PAGE_SIZE )
		remap_pages = (ph->fsize + PAGE_SIZE) >> PAGE_BITS;
	    else
		remap_pages = ph->fsize >> PAGE_BITS;
	    dst = ph->vaddr & phys_mask;

	    // Estimate the kernel virtual address offset.
	    get_vcpu().set_kernel_vaddr( ph->vaddr - dst );

	    word_t ph_end_addr = dst + remap_pages*PAGE_SIZE;
	    if( ph_end_addr > end_addr )
		end_addr = ph_end_addr;

	    if( debug_elf )
		con << "  Source " << (void *)src
		    << ", size " << (void *)ph->fsize
		    << " --> vaddr " << (void *)ph->vaddr
		    << " [" << (void *)dst << "]"
		    << ", remap pages " << remap_pages
		    << '\n';
	    if( (src % PAGE_SIZE) != (ph->vaddr % PAGE_SIZE) )
		PANIC( "ELF object is not page aligned." );

	    xen_memory.remap_boot_region( src, remap_pages, dst );
	}
    }

    // Detect a RAMDISK.
    word_t elf_file_size = ehdr->get_file_size();
    ramdisk_start = xen_start_info.mod_start + elf_file_size;
    ramdisk_start = (ramdisk_start + PAGE_SIZE - 1) & PAGE_MASK;
    if( ramdisk_start < (xen_start_info.mod_start + xen_start_info.mod_len) )
    {
	// We have a RAMDISK.
	ramdisk_len = 
	    xen_start_info.mod_start + xen_start_info.mod_len - ramdisk_start;

	con << "RAMDISK detected, size " << ramdisk_len << " bytes\n";
	if( debug_ramdisk ) {
	    con << "ELF kernel size: " << elf_file_size << " bytes\n";
	    con << "RAMDISK start: " << (void *)ramdisk_start << '\n';
	}

	// TODO: we arbitrarily give 1MB of space between the guest kernel
	// and its RAMDISK.  It seems to work, but is it appropriate?
	word_t ramdisk_target = end_addr + MB(1);
	xen_memory.remap_boot_region( ramdisk_start, ramdisk_len >> PAGE_BITS,
		ramdisk_target );
	ramdisk_start = ramdisk_target;
    }
    else
	ramdisk_len = 0;

    if( !frontend_elf_rewrite(ehdr, get_vcpu().get_kernel_vaddr(), false) )
	PANIC( "Failure loading the guest OS." );

    return phys_entry;
}
#endif

#ifdef CONFIG_XEN_2_0
static void init_xen_iopl()
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    dom0_op_t dom0_op;
    dom0_op.cmd = DOM0_IOPL;
    dom0_op.interface_version = DOM0_INTERFACE_VERSION;
    dom0_op.u.iopl.domain = 0;
    dom0_op.u.iopl.iopl = 1;
    if( XEN_dom0_op(&dom0_op) )
	PANIC( "Unable to configure the Xen IOPL." );
#endif
}
#else
static void init_xen_iopl()
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    physdev_op_t physdev_op;
    physdev_op.cmd = PHYSDEVOP_SET_IOPL;
    physdev_op.u.set_iopl.iopl = 1;
    if( XEN_physdev_op(&physdev_op) )
	PANIC( "Unable to configure the Xen IOPL." );
#endif
}
#endif

static word_t query_total_mach_mem()
    // Return the total amount of memory that the real machine has installed.
{
    if( !xen_start_info.is_privileged_dom() )
	return 0;

    // the dom0_op has been deprecated and there is no obvious replacement
    // XXX passthru code checks this!!
#ifndef DOM0_PHYSINFO
#ifdef CONFIG_DEVICE_PASSTHRU
#error "Need to fix this first!"
#endif
    return 0;
#else
    dom0_op_t dom0_op;
    dom0_op.cmd = DOM0_PHYSINFO;
    dom0_op.interface_version = DOM0_INTERFACE_VERSION;
    if( XEN_dom0_op(&dom0_op) )
	PANIC( "Unable to query the Xen physical information." );
    return dom0_op.u.physinfo.total_pages << PAGE_BITS;
#endif
}


void afterburn_main( start_info_t *start_info, word_t boot_stack )
{
    // Keep a copy of the start info, because eventually we'll reclaim
    // the original page allocated for it.  We need to have access
    // to the start info and the shared info to enable console output
    // for secondary domains.  Console output via a hypercall is only
    // possible for the initial domain.
    memcpy( &xen_start_info, start_info, sizeof(xen_start_info) );
    map_shared_info( start_info->shared_info );
    xen_controller.init();

    hiostream_kaxen_t con_driver;
    con_driver.init();
    con.init( &con_driver, CONFIG_CONSOLE_PREFIX ": " );
    console_init( xen_putc, "\e[1m\e[37m" CONFIG_CONSOLE_PREFIX ":\e[0m " );

    printf( "--- kaxen --- http://l4ka.org --- University of Karlsruhe ---\n" );
    printf( "Built on "__DATE__" "__TIME__ " using gcc version "__VERSION__".\n");
#if defined(CONFIG_DEBUGGER)
    printf( "--- Press " CONFIG_DEBUGGER_BREAKIN_KEY_NAME 
	    " to enter the interactive debugger. ---\n" );
#endif


    vcpu.cpu_id = 0;
    vcpu.cpu_hz = xen_shared_info.get_cpu_freq();
    vcpu.xen_esp0 = 0;
    vcpu.set_kernel_vaddr( 0 );

    // VCPU must be non-zero MHz for time calculation
    ASSERT(vcpu.cpu_hz > 10000000);

    dump_xen_start_info();
    dump_xen_shared_info();

    xen_memory.init( query_total_mach_mem() );

#if 0
    get_burn_symbols().init();
    if( !frontend_init(&vcpu.cpu) )
	return;

#if defined(CONFIG_DEVICE_APIC)
    lapic.set_id(vcpu.cpu_id);
    vcpu.cpu.set_lapic(&lapic);
    con << "VCPU set LAPIC @ " << vcpu.cpu.get_lapic() 
	<< ", ID to " << vcpu.cpu.get_lapic()->get_id()
	<< "\n";
    ASSERT(sizeof(local_apic_t) == 4096); 
#endif
    
    init_xen_traps();
    init_xen_callbacks();

#if defined(CONFIG_DEVICE_APIC)
    // Initialize ACPI parser.
    acpi.init(vcpu.cpu_id);
    init_io_apics();
#endif

    word_t ramdisk_start, ramdisk_len, entry_ip;
    entry_ip = map_guest_modules( ramdisk_start, ramdisk_len );
    xen_memory.alloc_remaining_boot_pages();
    xen_memory.init_m2p_p2m_maps();

    
    con << "Memory consumed by the wedge: " 
	<< xen_memory.get_wedge_size() / KB(1) << "K, "
	<< xen_memory.get_wedge_size() / MB(1) << '.'
	<< (xen_memory.get_wedge_size() % MB(1)) / KB(100) << "M\n";
    con << "Memory remaining for the guest OS: "
	<< xen_memory.get_guest_size() / KB(1) << "K, "
	<< xen_memory.get_guest_size() / MB(1)  << '.'
	<< (xen_memory.get_guest_size() % MB(1)) / KB(100) << "M\n";

#ifdef CONFIG_VMI_SUPPORT
    void vmi_init( void );
    vmi_init();
#endif
#endif

    init_xen_iopl();

#if 0
    guest_os_boot( entry_ip, ramdisk_start, ramdisk_len );
#endif
}

NORETURN void panic( xen_frame_t *frame )
{
    while( 1 ) {
	printf( "Panic, stopping and trying to enter kernel debugger.\n" );
#if 0 // TODO amd64
	DEBUGGER_ENTER( frame );
#endif
	XEN_yield();
    };
}

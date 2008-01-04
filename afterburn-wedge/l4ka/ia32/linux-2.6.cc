/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/ia32/linux-2.6.cc
 * Description:   Linux preboot initialization.  Some of the init
 *                values are extracted from the research resource monitor.
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
 * $Id: linux-2.6.cc,v 1.14 2006/09/21 14:02:40 joshua Exp $
 *
 ********************************************************************/

#include <memory.h>
#include <bind.h>
#include INC_ARCH(cpu.h)
#include INC_WEDGE(vcpulocal.h)
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(resourcemon.h)
#include <debug.h>
#include INC_WEDGE(vm.h)


/*
 * At boot, cs:2 is the setup header.  We must initialize the
 * setup header, and then jump to startup_32.  It is located at
 * 0x9020:2.  But we can safely locate it anywhere between the link address
 * and the start of the text segment.
 *
 * Additionally, esi must point at the structure.
 *
 * See arch/i386/boot/setup.S
 */

static const unsigned linux_boot_param_addr = 0x9022;
static const unsigned linux_boot_param_size = 2048;
static const unsigned linux_cmdline_addr = 0x10000;
static const unsigned linux_cmdline_size = 256;
static const unsigned linux_e820map_max_entries = 32;

typedef enum {
    ofs_screen_info=0, ofs_cmdline=0x228,
    ofs_e820map=0x2d0, ofs_e820map_nr=0x1e8,
    ofs_initrd_start=0x218, ofs_initrd_size=0x21c,
    ofs_loader_type=0x210,
} boot_offset_e;

struct screen_info_t
{
    u8_t  orig_x;		/* 0x00 */
    u8_t  orig_y;		/* 0x01 */
    u16_t dontuse1;		/* 0x02 -- EXT_MEM_K sits here */
    u16_t orig_video_page;	/* 0x04 */
    u8_t  orig_video_mode;	/* 0x06 */
    u8_t  orig_video_cols;	/* 0x07 */
    u16_t unused2;		/* 0x08 */
    u16_t orig_video_ega_bx;	/* 0x0a */
    u16_t unused3;		/* 0x0c */
    u8_t  orig_video_lines;	/* 0x0e */
    u8_t  orig_video_isVGA;	/* 0x0f */
    u16_t orig_video_points;	/* 0x10 */
};

struct e820_entry_t
{
    enum e820_type_e {e820_ram=1, e820_reserved=2, e820_acpi=3, e820_nvs=4};

    u64_t addr;
    u64_t size;
    u32_t type;
};

static void e820_init( void )
    // http://www.acpi.info/
{
    e820_entry_t *entries = (e820_entry_t *)
	(linux_boot_param_addr + ofs_e820map);
    u8_t *nr_entries = (u8_t *)(linux_boot_param_addr + ofs_e820map_nr);

    if( resourcemon_shared.phys_size <= MB(1) )
	return;

    // Declare RAM for 0 to 640KB.
    entries[0].addr = 0;
    entries[0].size = KB(640);
    entries[0].type = e820_entry_t::e820_ram;

    // Reserve 640K to 1MB region.
    entries[1].addr = KB(640);
    entries[1].size = MB(1) - entries[1].addr;
    entries[1].type = e820_entry_t::e820_reserved;

#if defined(CONFIG_WEDGE_STATIC)
    entries[2].addr = entries[1].addr + entries[1].size;
    entries[2].size = resourcemon_shared.phys_size - entries[2].addr;
    entries[2].type = e820_entry_t::e820_ram;

    *nr_entries = 3;
#else
    // Declare RAM for 1MB to the wedge.
    entries[2].addr = MB(1);
    
    entries[2].size = get_vcpu().get_wedge_paddr() - entries[2].addr;
    entries[2].type = e820_entry_t::e820_ram;

    // Reserve the wedge's memory.
    entries[3].addr = get_vcpu().get_wedge_paddr();
    entries[3].size = get_vcpu().get_wedge_end_paddr() - entries[3].addr;
    entries[3].type = e820_entry_t::e820_reserved;

    // Declare the remainder of the memory.
    entries[4].addr = entries[3].addr + entries[3].size;
    entries[4].size = resourcemon_shared.phys_size - entries[4].addr;
    entries[4].type = e820_entry_t::e820_ram;

    *nr_entries = 5;

#if defined(CONFIG_DEVICE_PASSTHRU)
#warning jsXXX: declare devices from resourcemon_shared
    // Declare all of machine memory, so that it has a representation in
    // the page map, but reserved.
    if( resourcemon_shared.phys_end > resourcemon_shared.phys_size )
    {
	entries[5].addr = resourcemon_shared.phys_size;
	entries[5].size = resourcemon_shared.phys_end+1 - resourcemon_shared.phys_size - PAGE_SIZE;
	entries[5].type = e820_entry_t::e820_reserved;

	entries[6].addr = entries[5].addr + entries[5].size;
	entries[6].size = PAGE_SIZE;
	entries[6].type = e820_entry_t::e820_ram;

        *nr_entries = 7;
    }
#endif
#endif
}

void ramdisk_init( void )
{
    word_t *start = (word_t *)(linux_boot_param_addr + ofs_initrd_start);
    word_t *size  = (word_t *)(linux_boot_param_addr + ofs_initrd_size);

	
    // Linux wants a physical start address.
    *start = resourcemon_shared.ramdisk_start;
    if( resourcemon_shared.ramdisk_start >= resourcemon_shared.link_vaddr ) {
	// The resource monitor configured the ramdisk with a virtual addr.
	*start -= resourcemon_shared.link_vaddr;
    }

    *size = resourcemon_shared.ramdisk_size;
    printf( "Initialize ramdisk start %x file size %08d bytes\n", *start, *size);
}

bool backend_preboot( backend_vcpu_init_t *init_info )
{
    get_cpu().cr0.x.fields.pe = 1;	// Enable protected mode.

    // Zero the parameter block.
    init_info->entry_param = (u8_t *)linux_boot_param_addr;
    for( unsigned i = 0; i < linux_boot_param_size; i++ )
	init_info->entry_param[i] = 0;

    // Choose an arbitrary loader identifier.
    *(u8_t *)(linux_boot_param_addr + ofs_loader_type) = 0x14;

    // Init the screen info.
    screen_info_t *scr = (screen_info_t *)(init_info->entry_param + ofs_screen_info);
    scr->orig_x = 0;
    scr->orig_y = 0;
    scr->orig_video_cols = 80;
    scr->orig_video_lines = 25;
    scr->orig_video_points = 16;
    scr->orig_video_mode = 3;
    scr->orig_video_isVGA = 1;
    scr->orig_video_ega_bx = 1;

    // Install the command line.
    char *src_cmdline = get_vm()->cmdline;
    
    ASSERT( linux_cmdline_addr > 
	    linux_boot_param_addr + linux_boot_param_size );
    char *cmdline = (char *)linux_cmdline_addr;
    unsigned cmdline_len = 1 + strlen( src_cmdline );
    if( cmdline_len > linux_cmdline_size )
	cmdline_len = linux_cmdline_size;
    memcpy( cmdline, src_cmdline, cmdline_len );
    *(char **)(init_info->entry_param + ofs_cmdline) = cmdline;

    e820_init();
    ramdisk_init();

    return true;
}


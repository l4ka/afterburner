/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     linux-2.6.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <string.h>
#include <l4/ia32/virt.h>
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(backend.h)
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
    ofs_screen_info = 0,
    ofs_e820map_nr = 0x1e8,
    ofs_loader_type = 0x210,
    ofs_initrd_start = 0x218,
    ofs_initrd_size = 0x21c,
    ofs_cmdline = 0x228,
    ofs_e820map = 0x2d0,
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

static void e820_init(  vm_t *vm )
    // http://www.acpi.info/
{
    e820_entry_t *entries = (e820_entry_t *) (linux_boot_param_addr + ofs_e820map);
    u8_t *nr_entries = (u8_t *) (linux_boot_param_addr + ofs_e820map_nr);

    // available physical memory should be more than 1MB
    ASSERT( vm->gphys_size > MB(1) );
    
    // declare RAM for 0 to 640KB.
    entries[0].addr = 0;
    entries[0].size = KB(640);
    entries[0].type = e820_entry_t::e820_ram;

    // reserve 640K to 1MB region.
    entries[1].addr = KB(640);
    entries[1].size = MB(1) - entries[1].addr;
    entries[1].type = e820_entry_t::e820_reserved;

    // declare rest as RAM
    entries[2].addr = MB(1);
    entries[2].size = vm->gphys_size - entries[2].addr;
    entries[2].type = e820_entry_t::e820_ram;

    *nr_entries = 3;
}

void ramdisk_init( vm_t *vm )
{
    word_t *start = (word_t *) (linux_boot_param_addr + ofs_initrd_start);
    word_t *size  = (word_t *) (linux_boot_param_addr + ofs_initrd_size);

    // Linux wants a physical start address.
    *start = vm->ramdisk_start;
    *size = vm->ramdisk_size;
}

bool backend_preboot( backend_vcpu_init_t *init_info )
{
    vcpu_t &vcpu = get_vcpu();
    vm_t *vm = vcpu.main_info->mr_save.get_vm();

    u8_t *param_block = (u8_t *) linux_boot_param_addr;
    
    // Zero the parameter block.
    for( unsigned i = 0; i < linux_boot_param_size; i++ )
	param_block[i] = 0;

    // Choose an arbitrary loader identifier.
    param_block[ofs_loader_type] = 0x14;

    // Init the screen info.
    screen_info_t *scr = (screen_info_t *)(param_block + ofs_screen_info);
    scr->orig_x = 0;
    scr->orig_y = 0;
    scr->orig_video_cols = 80;
    scr->orig_video_lines = 25;
    scr->orig_video_points = 16;
    scr->orig_video_mode = 3;
    scr->orig_video_isVGA = 1;
    scr->orig_video_ega_bx = 1;

    // Install the command line.
    char *src_cmdline = vm->cmdline;

    ASSERT( linux_cmdline_addr > linux_boot_param_addr + linux_boot_param_size );
    char *cmdline = (char *) linux_cmdline_addr;
    unsigned cmdline_len = 1 + strlen( src_cmdline );
    if( cmdline_len > linux_cmdline_size ) {
	con << "command line too long, truncating\n";
	
	cmdline_len = linux_cmdline_size;
	src_cmdline[cmdline_len] = 0;
    }
    memcpy( cmdline, src_cmdline, cmdline_len );
    *(char **)(param_block + ofs_cmdline) = (char*) linux_cmdline_addr;

    e820_init( vm );
    ramdisk_init( vm );
    
    return true;
}


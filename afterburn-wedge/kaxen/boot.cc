/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/boot.cc
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
#include <console.h>
#include <string.h>
#include INC_WEDGE(wedge.h)

static const bool debug_multiboot = true; // XXX use debug_id_t in debug.h?

static void guest_multiboot_boot( word_t entry_ip, word_t ramdisk_start,
                           word_t ramdisk_len, unsigned skip )
{
    UNIMPLEMENTED();
}

static void guest_multiboot_inspect( word_t start, unsigned skip )
{
    UNIMPLEMENTED();
}

#ifdef CONFIG_ARCH_AMD64
#include <../contrib/multiboot.h>
#include <elfsimple.h>

static multiboot_header_t mbh;
static word_t entry64;

#include INC_WEDGE(memory.h)
static char mbi_hi[PAGE_SIZE] __attribute__((__aligned__(PAGE_SIZE)));
static void guest_mb64_boot( word_t entry_ip, word_t ramdisk_start,
                      word_t ramdisk_len, unsigned skip )
{
    get_cpu().cr0.x.fields.pe = 1;	// Enable protected mode.
    get_cpu().cr0.x.fields.pg = 1;      // Enable paging.
    get_cpu().disable_interrupts();

    // get us a page at low addresses
    static const word_t low_addr = 0x100a000;
    xen_memory.remap_boot_region( (word_t)&mbi_hi, 1, low_addr, false );
    multiboot_info_t* mbi = (multiboot_info_t*)low_addr;
    memset( mbi, 0, PAGE_SIZE );

    // fake multiboot info
    mbi -> flags = MULTIBOOT_INFO_FLAG_MEM
                 | MULTIBOOT_INFO_FLAG_CMDLINE
                 | MULTIBOOT_INFO_FLAG_MODS;
    mbi -> mem_lower = 0;  // XXX
    mbi -> mem_upper = 0;  // XXX
    char* cmdline = (char*)(low_addr + sizeof(*mbi));
    const char* prefix = "afterburn-guest ";
    memcpy( cmdline, prefix, strlen( prefix ) );
    memcpy( cmdline + strlen( prefix ), xen_start_info.cmd_line + skip,
            strlen( (const char*)(xen_start_info.cmd_line + skip) ) );
    module_t* mods = (module_t*)(low_addr + sizeof(mbi) + strlen(cmdline) + 100); // XXX why +100?
    mbi -> cmdline = (word_t)cmdline;
    mbi -> mods_count = 0;
    mbi -> mods_addr = (word_t)mods;

#define BMERGE_MAGIC1 0x60D019817CB41EC8
#define BMERGE_MAGIC2 0x271918E0339D139B
#define MAX_MODS 32

    // We use a hacky cooked-up format to flatten several multiboot modules
    // into one file. Essentially, the main kernel image is followed by
    // another extension header on the next page, BMERGE_MAGIC1 must be the
    // first eight bytes of this page. Then any number of entries may follow,
    // entries are 16 bytes in size. The first quadword is the offset of the
    // beginning of the specified module, relative to the first page (i.e.
    // the address of the first magic value), the second quadword is the
    // offset of the end of the module, the third the offset of it's command
    // line, encoded similarly. The table is closed by another magic value.

    word_t* ramdisk = (word_t*)ramdisk_start;
    if(ramdisk_len > 16 && ramdisk[0] == BMERGE_MAGIC1) {
        printf("Extended ramdisk detected!\n");
                                                        // sanity check
        unsigned i;
        for( i = 1;ramdisk[i] != BMERGE_MAGIC2 && i < MAX_MODS;
             i += 3 ) {
            mods[mbi->mods_count].mod_start = ramdisk_start + ramdisk[i];
            mods[mbi->mods_count].mod_end = ramdisk_start + ramdisk[i+1];
            mods[mbi->mods_count].string = ramdisk_start + ramdisk[i+2];
            mods[mbi->mods_count].reserved = 0;
            mbi->mods_count++;
        }
        if( ramdisk[i] != BMERGE_MAGIC2 )
            PANIC( "Magic2 not found??\n" );
    }
    else if( ramdisk_len != 0 )
        PANIC( "Ill-formed ramdisk!\n" );

    printf( "starting os (\"%s\") @ %p\n",
            cmdline, entry_ip );
    ((void(*)(u32_t, u32_t))entry64)(MULTIBOOT_BOOTLOADER_MAGIC, low_addr);
    UNIMPLEMENTED();
}

static void guest_mb64_inspect( word_t start, unsigned skip )
{
    // For now, this is the only documentation of the mb64 format:
    // mb64 is intended to be a minimalistic extension of the multiboot
    // standard to accomodate booting into paged long mode. It is
    // equivalent to the multiboot standard, except for the following
    // points:
    // 1. The multiboot header MUST live quadword-aligned.
    // 2. The flags 2 and 16 in the multiboot header MUST NOT be used.
    // 3. The multiboot header MUST be followed by 32 bits of padding
    //    and N quadwords of extension header.
    //    o The first word of the extension header MUST be the additional
    //      magic constant 0x2E6F4BAD69095394.
    //    o The second word of the extension header is an alternative entry
    //      point.
    //    o The 3rd to (N-1)th words are phdr patchup words. The nth word
    //      corresponds to the phdr (n-3). The phdr patchup is added to the
    //      phdr load address, except if it is ~0, then the respective phdr
    //      is not loaded at all. (Segments removed in this way should not
    //      be referenced in any of the annotation sections.) There MUST
    //      NOT be more phdr patchup words than there are phdrs in the elf
    //      image file.
    //        The intention of this is that a) certain ad-hoc relocations
    //        the kernel usually does before entering long mode can be
    //        accomodated for by the loader, and that b) startup code only
    //        belonging to the multiboot32 path can be removed so that it
    //        doesn't interfere with the rest of the kernel and wedge.
    //        Use the .afterburn_noannotate and .afterburn_annotate
    //        pseudo-opcode to stop and re-start annotation generation so
    //        that no annotations are generated for removed segments.
    //    o The last (Nth) word MUST be the magic constant
    //      0x43BB4C1653535AB7.
    // 4. Instead of starting the OS at the elf entry point, it MUST BE
    //    started at the alternative entry point. The bootloader magic
    //    value MUST be passed in %rdi (arg1), the virtual address of the
    //    multiboot information structure MUST BE passed in %rsi (arg2).
    //    Both of these MUST be considered as zero-extendet 32 bit values.
    // 5. The initial machine state MUST be as follows:
    //    o The processor runs in long mode enabled, long mode activated.
    //    o There is an initial page table hierarchy.
    //    o MORE FORMAL GIBBERISH YET TO COME.

#define MB64_MAGIC1 0x2E6F4BAD69095394
#define MB64_MAGIC2 0x43BB4C1653535AB7

    ASSERT( start % 8 == 0 );
    for( unsigned* i = (unsigned*)start;i < (unsigned*)(start + 8192);i+=2 )
        if( i[0] == MULTIBOOT_HEADER_MAGIC ) { // header found?
            if( i[0] + i[1] + i[2] != 0 ) // checksum invalid
                continue;

            if( i[1] & (1 << 2) || i[1] & (1 << 16) )
                PANIC( "mb64 header using deprecated flags!" );

            // save header
            memcpy( &mbh, i, sizeof( mbh ) );

            // here come the mb64 extensions
            word_t* ext = (word_t*)(i + 4);
            if( ext[0] != MB64_MAGIC1 )
                PANIC( "Not a mb64 header!" );
            entry64 = ext[1];

            elf_ehdr_t *ehdr = elf_is_valid( start );
            ASSERT( ehdr ); // we cannot be called without validating first
            for( unsigned j = 0;;++j ) {
                if( j > ehdr->phnum )
                    PANIC( "Invalid mb64 header!\n" );
                if( ext[j+2] == MB64_MAGIC2 ) // we're done
                    break;

                if( ext[j+2] == ~0ul ) // don't load that phdr
                    ehdr->get_phdr_table()[j].type = PT_NULL;
                else
                    ehdr->get_phdr_table()[j].vaddr += ext[j+2];
            }

            if( debug_multiboot ) {
                printf( "Found mb64 header @ %p:\n", i );
                printf( "  flags: %08x\n", mbh.flags );
                printf( "  entry: %p\n", entry64 );
            }

            return;
        }

    PANIC( "No mb64 header found!" );
}
#endif

void guest_inspect( word_t start )
{
    const char* cmdl = (const char*)xen_start_info.cmd_line;
    unsigned n = 0;
    while( cmdl[n] && cmdl[n] != ' ' )
        ++n;
    if( !strncmp( cmdl, "multiboot", n ) )
        guest_multiboot_inspect( start, n );
#ifdef CONFIG_ARCH_AMD64
    else if( !strncmp( cmdl, "mb64", n ) )
        guest_mb64_inspect( start, n );
#endif
}

void guest_os_boot( word_t entry_ip, word_t ramdisk_start,
                    word_t ramdisk_len )
{
    const char* cmdl = (const char*)xen_start_info.cmd_line;
    unsigned n = 0;
    while( cmdl[n] && cmdl[n] != ' ' )
        ++n;
    if( !strncmp(cmdl, "linux", n ) ) {
        printf( "Using linux booting protocol.\n" );
        guest_linux_boot( entry_ip, ramdisk_start, ramdisk_len, n );
    }
    else if( !strncmp( cmdl, "multiboot", n ) ) {
        printf( "Using multiboot booting protocol.\n" );
        guest_multiboot_boot( entry_ip, ramdisk_start, ramdisk_len, n );
    }
#ifdef CONFIG_ARCH_AMD64
    else if( !strncmp( cmdl, "mb64", n ) ) {
        printf( "Using mb64 booting protocol.\n" );
        guest_mb64_boot( entry_ip, ramdisk_start, ramdisk_len, n );
    }
#endif

    // fallback
    printf( "Falling back to linux booting protocol...\n" );
    guest_linux_boot( entry_ip, ramdisk_start, ramdisk_len, 0 );
}

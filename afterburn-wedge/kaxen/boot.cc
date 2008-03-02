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

void guest_multiboot_boot( word_t entry_ip, word_t ramdisk_start,
                           word_t ramdisk_len, unsigned skip )
{
    UNIMPLEMENTED();
}

void guest_mb64_boot( word_t entry_ip, word_t ramdisk_start,
                      word_t ramdisk_len, unsigned skip )
{
    printf( "starting os (\"%s\")\n",
            ((const char*)xen_start_info.cmd_line) + skip );
    ((void(*)())entry_ip)();
    UNIMPLEMENTED();
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
    else if( !strncmp( cmdl, "mb64", n ) ) {
        printf( "Using mb64 booting protocol.\n" );
        guest_mb64_boot( entry_ip, ramdisk_start, ramdisk_len, n );
    }

    // fallback
    printf( "Falling back to linux booting protocol...\n" );
    guest_linux_boot( entry_ip, ramdisk_start, ramdisk_len, 0 );
}

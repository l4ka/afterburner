/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/startup.cc
 * Description:   C runtime initialization.  This has *the* entry point
 *                invoked by the boot loader.
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
 * $Id: startup.cc,v 1.12 2005/09/02 20:24:31 joshua Exp $
 *
 ********************************************************************/

#include INC_ARCH(types.h)
#include <l4/kdebug.h>
#include "l4ka/resourcemon.h"

extern u8_t _start_vcpulocal[], _end_vcpulocal[],  _sizeof_vcpulocal[],
    _start_vcpulocal_shadow[], _end_vcpulocal_shadow[];
word_t	 start_vcpulocal = (word_t) _start_vcpulocal;
word_t	 end_vcpulocal = (word_t) _end_vcpulocal;
word_t	 sizeof_vcpulocal = (word_t) _sizeof_vcpulocal;
word_t	 start_vcpulocal_shadow = (word_t) _start_vcpulocal_shadow;
word_t	 end_vcpulocal_shadow = (word_t) _end_vcpulocal_shadow;


extern void afterburn_main();

static void prezero( void )
{
    extern unsigned char afterburn_prezero_start[], afterburn_prezero_end[];
    extern unsigned char _start_bss[], _end_bss[];
    unsigned char *prezero;

    for( prezero = &afterburn_prezero_start[sizeof(unsigned char *)];
	    prezero < afterburn_prezero_end; prezero++ )
	*prezero = 0;

    // Handle the bss.
    for( prezero = _start_bss; prezero < _end_bss; prezero++ )
	*prezero = 0;
}

static void ctors_exec( void )
{
    extern void (*afterburn_ctors_start)(void);
    void (**ctors)(void) = &afterburn_ctors_start;

    // ctors start *after* the afterburn_ctors_start symbol.
    for( unsigned int i = 1; ctors[i]; i++ )
	ctors[i]();
}

static void dtors_exec( void )
{
    extern void (*afterburn_dtors_start)(void);
    void (**dtors)(void) = &afterburn_dtors_start;

    // dtors start *after the afterburn_dtors_start symbol.
    for( unsigned int i = 1; dtors[i]; i++ )
	dtors[i]();
}

extern "C" NORETURN void afterburn_c_runtime_init( void )
{
    prezero();
    ctors_exec();

    afterburn_main();

    dtors_exec();

    while( 1 ) {
	L4_KDB_Enter("afterburn exited");
    }
}

// Put the stack in a special section so that clearing bss doesn't clear
// the stack.
static unsigned char afterburn_first_stack[KB(16)] 
	SECTION(".data.stack") ALIGNED(CONFIG_STACK_ALIGN);

IResourcemon_startup_config_t resourcemon_startup_config 
	SECTION(".resourcemon.startup") =
{
    version: IResourcemon_version,
    start_ip: (L4_Word_t)afterburn_c_runtime_init,
    start_sp: (L4_Word_t)afterburn_first_stack + sizeof(afterburn_first_stack) - CONFIG_STACK_SAFETY,
};

IResourcemon_shared_t resourcemon_shared
	SECTION(".resourcemon") =
{
    version: IResourcemon_version,
    cpu_cnt: 1,
};

unsigned char afterburn_utcb_area[CONFIG_UTCB_AREA_SIZE] 
	SECTION(".l4utcb") ALIGNED(CONFIG_UTCB_AREA_SIZE) = { 1 };
unsigned char afterburn_kip_area[CONFIG_KIP_AREA_SIZE]
	SECTION(".l4kip") ALIGNED(CONFIG_KIP_AREA_SIZE) = { 1 };


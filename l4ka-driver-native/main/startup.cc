/*********************************************************************
 *
 * Copyright (C) 2005-2006  University of Karlsruhe
 *
 * File path:     main/startup.cc
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
 ********************************************************************/

#include <common/resourcemon.h>
#include <l4/kdebug.h>

extern void drivers_main();

static void prezero( void )
{
    extern unsigned char _bss_start[], _bss_end[];
    unsigned char *prezero;

    // Handle the bss.
    for( prezero = _bss_start; prezero < _bss_end; prezero++ )
	*prezero = 0;
}

static void ctors_exec( void )
{
    extern void (*ctors_start)(void);
    void (**ctors)(void) = &ctors_start;

    // ctors start *after* the ctors_start symbol.
    for( unsigned int i = 1; ctors[i]; i++ )
	ctors[i]();
}

static void dtors_exec( void )
{
    extern void (*dtors_start)(void);
    void (**dtors)(void) = &dtors_start;

    // dtors start *after the dtors_start symbol.
    for( unsigned int i = 1; dtors[i]; i++ )
	dtors[i]();
}

extern "C" NORETURN void c_runtime_init( void )
{
    prezero();
    ctors_exec();

    drivers_main();

    dtors_exec();

    while( 1 ) {
	L4_KDB_Enter("exited");
    }
}

// Put the stack in a special section so that clearing bss doesn't clear
// the stack.
static unsigned char first_stack[KB(16)] 
	SECTION(".stack.first") ALIGNED(ARCH_STACK_ALIGN);

IResourcemon_startup_config_t resourcemon_startup_config 
	SECTION(".resourcemon.startup") =
{
    version: IResourcemon_version,
    start_ip: (L4_Word_t)c_runtime_init,
    start_sp: (L4_Word_t)first_stack + sizeof(first_stack) - ARCH_STACK_SAFETY,
};

IResourcemon_shared_t resourcemon_shared
	SECTION(".resourcemon") =
{
    version: IResourcemon_version,
    cpu_cnt: 1,
};

#define UTCB_AREA_SIZE	KB(4)
#define KIP_AREA_SIZE	KB(4)

unsigned char utcb_area[UTCB_AREA_SIZE] 
	SECTION(".l4utcb") ALIGNED(UTCB_AREA_SIZE) = { 1 };
unsigned char kip_area[KIP_AREA_SIZE]
	SECTION(".l4kip") ALIGNED(KIP_AREA_SIZE) = { 1 };


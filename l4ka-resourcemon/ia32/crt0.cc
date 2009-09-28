/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     l4ka-ia32/crt0.cc
 * Description:   Initializes the C runtime, and then calls main().
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
#include <l4/kip.h>
#include <l4/kdebug.h>
#include <string.h>

void *l4_kip;
L4_Bool_t l4_hsched_enabled = false, l4_pmsched_enabled = false, l4_tracebuffer_enabled = false,
    l4_logging_enabled = false, l4_iommu_enabled = false, l4_smallspaces_enabled = false;
L4_Word_t l4_cpu_cnt, l4_user_base;


// The _start entry point, where execution begins, and no assumptions exist.
__asm__ (
	".text;"
	".global _start;"
	"_start:;"

	// Clear the bss (which clears the stack, so must use assembler).
	"mov	$__bss_start, %eax;"
	"cmp	$_end, %eax;"
	"jae	1f;"
	"99:"
	"movl	$0x0, (%eax);"
	"add	$0x4, %eax;"
	"cmp	$_end, %eax;"
	"jb	99b;"

	// Install the stack and enter C code.
	"1:"
	"movl (resourcemon_first_stack), %esp;"	// Install the stack.
	"pushl $2f;"				// Install a return address.
	"jmp c_runtime_init;"			// Enter C code.
	"2: jmp 2b;"				// Infinite loop.
	);


static void ctors_exec( void )
{
    l4_kip = L4_GetKernelInterface();
    l4_cpu_cnt = L4_NumProcessors(l4_kip);
    char *l4_feature;

    for( L4_Word_t i = 0; (l4_feature = L4_Feature(l4_kip,i)) != '\0'; i++ )
    {
	if( !strcmp("logging", l4_feature) )
            l4_logging_enabled = true;
	if( !strcmp("pmscheduling", l4_feature) )
            l4_pmsched_enabled = true;
	if( !strcmp("hscheduling", l4_feature) )
            l4_hsched_enabled = true;
	if( !strcmp("iommu", l4_feature) )
            l4_iommu_enabled = true;
	if( !strcmp("smallspaces", l4_feature) )
            l4_smallspaces_enabled = true;
	if( !strcmp("tracebuffer", l4_feature) )
            l4_tracebuffer_enabled = true;
    }

    extern void (*__ctors_start)(void);
    void (**ctors)(void) = &__ctors_start;

    // ctors start *after* the __ctors_start symbol.
    for( unsigned int i = 1; ctors[i]; i++ )
	ctors[i]();
}

static void dtors_exec( void )
{
    extern void (*__dtors_start)(void);
    void (**dtors)(void) = &__dtors_start;

    // dtors start *after* the __dtors_start symbol.
    for( unsigned int i = 1; dtors[i]; i++ )
	dtors[i]();

}

extern "C" __attribute__ ((noreturn)) void c_runtime_init( void )
{
    ctors_exec();

    extern int main();
    main();

    dtors_exec();

    while( 1 ) {
	L4_KDB_Enter("resourcemon exited");
    }
}



/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/startup.cc
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

#include INC_ARCH(types.h)
#include INC_WEDGE(xen_hypervisor.h)

extern void afterburn_main( start_info_t *xen_info, word_t boot_stack );


static void prezero( void )
{
    extern unsigned char afterburn_prezero_start[], afterburn_prezero_end[];
    unsigned char *prezero;

    // TODO: zero the afterburn's bss when dynamically loading the guest.

    for( prezero = &afterburn_prezero_start[sizeof(unsigned char *)];
	    prezero < afterburn_prezero_end; prezero++ )
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

extern "C" NORETURN void 
afterburn_c_runtime_init( start_info_t *xen_info, word_t boot_stack )
{
    prezero();
    ctors_exec();

    afterburn_main( xen_info, boot_stack );

    dtors_exec();

    while( 1 ) {
	XEN_yield();
    }
}

char xen_hypervisor_config_string[] SECTION("__xen_guest") =
  "GUEST_OS=kaxen,LOADER=generic,GUEST_VER=0.1"
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
  ",PT_MODE_WRITABLE"
#endif
#if defined(CONFIG_XEN_2_0)
  ",XEN_VER=2.0"
#elif defined(CONFIG_XEN_3_0)
  ",XEN_VER=xen-3.0"
#else
# error better fix that bootloader insanity
#endif
  ;
// Put the stack in a special section so that clearing bss doesn't clear
// the stack.
__asm__ (
    ".section .afterburn.stack,\"aw\"\n"
    ".balign 4\n"
    "kaxen_wedge_stack:\n"
    ".space 32*1024\n"
    "kaxen_wedge_stack_top:\n"
);

__asm__ (
    ".text\n"
    ".globl kaxen_wedge_start\n"
    "kaxen_wedge_start:\n"
    "	cld\n"
    "	mov	%esp, %eax\n" // Remember the stack provided by Xen.
    "	lea	kaxen_wedge_stack_top, %esp\n"
    "	push	%eax\n" // The stack provided by Xen.
    "	push	%esi\n" // Start info pointer.
    "	call	afterburn_c_runtime_init\n"
);


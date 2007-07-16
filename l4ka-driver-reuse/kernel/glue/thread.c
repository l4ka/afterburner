/*********************************************************************
 *
 * Copyright (C) 2004-2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/thread.c
 * Description:	Wrappers for manipulating L4 threads within
 * 		the L4Linux kernel.
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

#include <l4/kdebug.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "thread.h"
#include "wedge.h"

#define STACK_SIZE_PAGE_SHIFT	2
#define STACK_SIZE		(PAGE_SIZE << STACK_SIZE_PAGE_SHIFT)

EXPORT_SYMBOL(L4VM_thread_create);
EXPORT_SYMBOL(L4VM_thread_delete);

L4_ThreadId_t 
L4VM_thread_create( unsigned int gfp_mask, 
	void (*thread_func)(void *), int prio, int cpu,
	void *tlocal_data, unsigned tlocal_size )
{
    L4_ThreadId_t tid;
    L4_Word_t stack;

    if( !thread_func )
	return L4_nilthread;

    // Allocate a stack.
    if( tlocal_size > STACK_SIZE/2 )
	return L4_nilthread;
    stack = __get_free_pages( gfp_mask, STACK_SIZE_PAGE_SHIFT );
    if( stack == 0 )
	return L4_nilthread;

    tid = l4ka_wedge_thread_create( stack, STACK_SIZE, prio,
	    thread_func, tlocal_data, tlocal_size );
    if( L4_IsNilThread(tid) ) {
	free_pages( stack, STACK_SIZE_PAGE_SHIFT );
	L4_KDB_Enter("L4VM_thread_create: BUG");
    }

    return tid;
}

void
L4VM_thread_delete( L4_ThreadId_t tid )
{
    l4ka_wedge_thread_delete( tid );
}


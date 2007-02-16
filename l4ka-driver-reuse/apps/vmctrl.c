/*********************************************************************
 *                
 * Copyright (C) 2005, 2007,  University of Karlsruhe
 *                
 * File path:	vmctrl.c
 * Description:	Some common routines for interfacing with the resource monitor.
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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ucontext.h>

#include <l4/types.h>
#include <l4/kip.h>
#include "resourcemon_idl_client.h"

typedef void (*signal_handler_t)(int);

static void illegal_instruction_signal( int signo, struct sigcontext regs )
{
    L4_Word16_t instr = *(L4_Word16_t *)regs.eip;
    static const L4_Word16_t lock_nop = 0x90f0;

    if( instr != lock_nop ) {
    	signal( SIGILL, SIG_DFL );
	return;
    }

    // Emulate L4_GetKernelInterface() and return nil values.
    regs.eip += 2;
    regs.eax = 0;
    regs.ecx = 0;
    regs.edx = 0;
    regs.esi = 0;
}

void vmctrl_ignore_signals( void )
{
    L4_Word_t i;

    for( i = 0; i < _NSIG; i++ )
	signal( i, SIG_IGN );
}

int vmctrl_detect_l4( void )
{
    signal_handler_t old;
    void *kip;

    // On x86, L4_GetKernelInterface() is implemented with an illegal
    // instruction, and is thus a good way to test whether we are running on
    // L4 or not.
    old = signal( SIGILL, (signal_handler_t)illegal_instruction_signal );
    kip = L4_GetKernelInterface();
    if( old != SIG_ERR )
	signal( SIGILL, old );

    return NULL != kip;
}

L4_ThreadId_t vmctrl_get_root_tid( void )
{
    void *kip = L4_GetKernelInterface();
    // sigma0 = 0 + L4_ThreadIdUserBase(kip)
    // sigma1 = 1 + L4_ThreadIdUserBase(kip)
    // root task = 2 + L4_ThreadIdUserBase(kip)
    return L4_GlobalId( 2+L4_ThreadIdUserBase(kip), 1 );
}

L4_ThreadId_t vmctrl_lookup_service( guid_t guid )
{
    L4_ThreadId_t root_tid = vmctrl_get_root_tid();
    L4_ThreadId_t tid = L4_nilthread;
    CORBA_Environment env = idl4_default_environment;

    IResourcemon_query_interface( root_tid, guid, &tid, &env );
    if( env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &env );
	return L4_nilthread;
    }

    return tid;
}


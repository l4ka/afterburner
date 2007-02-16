/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:	l4ka-driver-reuse/apps/l4ka-ws-scan.c
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

#include <stdio.h>
#include <stdlib.h>

#include <l4/types.h>
#include <l4/kip.h>
#include "resourcemon_idl_client.h"
#include "vmctrl.h"

void usage( const char *progname )
{
    printf( "usage: %s <space id> <time interval (ms)> <total samples>\n",
	    progname );
    exit( 1 );
}

int main( int argc, const char *argv[] )
{
    CORBA_Environment env = idl4_default_environment;
    L4_ThreadId_t tid;
    L4_Word_t space_id, ms, samples;

    if( !vmctrl_detect_l4() ) {
	printf( "Error: this utility must run on L4!\n" );
	exit( 1 );
    }

    if( argc != 4 )
	usage( argv[0] );
    space_id = atol( argv[1] );
    ms = atol( argv[2] );
    samples = atol( argv[3] );
    printf( "Performing working set scan, space %lu, %lu ms time interval, "
	    "and %lu samples\n", space_id, ms, samples );

    vmctrl_ignore_signals();

    tid = vmctrl_lookup_service( UUID_IVMControl );
    if( L4_IsNilThread(tid) ) {
	printf( "Error: unable to locate the working set scanner.\n" );
	exit( 1 );
    }

    IVMControl_start_working_set_scan( tid, ms, samples, space_id, &env );
    if( env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &env );
	puts( "IPC failure to the resource monitor.\n" );
	return 1;
    }

    return 0;
}


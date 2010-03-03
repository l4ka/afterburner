/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     wsscan/wsscan.c
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
 * $Id$
 *                
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <l4/types.h>
#include "hypervisor_idl_client.h"

int main( int argc, char *argv[] )
{
    L4_ThreadId_t tid;
    CORBA_Environment env = idl4_default_environment;

    if( argc != 2 )
	exit(1);

    sscanf( argv[1], "%lx", &tid.raw );
    printf( "working set manager tid: 0x%lx\n", tid.raw );

    IVMControl_start_working_set_scan( tid, 90, 200, 1, &env );

    if( env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &env );
	fprintf( stderr, "ipc failure to the working set scanner.\n" );
	exit( 1 );
    }

    exit(0);
}


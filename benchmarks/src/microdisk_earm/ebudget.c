/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     microdisk_earm/ebudget.c
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
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/kip.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#define __USE_GNU	// for O_DIRECT
#include <fcntl.h>
#include <errno.h>
#include "earm_idl_client.h"
#include "resourcemon_idl_client.h"

void usage( char *progname )
{
    fprintf( stderr,  "usage: %s <resource> <domain> <value>\n", progname );
    exit( 1 );
}

int main( int argc, char *argv[] )
{
    L4_ThreadId_t earmmgr, locator;
    L4_MsgTag_t tag;
    L4_Msg_t msg;
    L4_Word_t logid = 0;
    L4_Word_t guid = 0;
    L4_Word_t budget = 0;
    CORBA_Environment ipc_env;

    if( argc < 3 )
	usage(argv[0]);
    
    guid = strtoul(argv[1], NULL, 0);
    logid = strtoul(argv[2], NULL, 0);
    budget = strtoul(argv[3], NULL, 0);

    locator = L4_GlobalId( 2+L4_ThreadIdUserBase(L4_GetKernelInterface()), 1 );
    printf("locator %p\n", locator.raw);
	
    ipc_env = idl4_default_environment;
    IResourcemon_query_interface( locator, UUID_IEarm_Manager, &earmmgr, &ipc_env );

    if( ipc_env._major == CORBA_USER_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -ENODEV;
    }
    else if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free(&ipc_env);
	return -EIO;
    }

    printf("earm mgr %p\n", earmmgr.raw);


    printf("Setting guid %d logid %d budget %d\n",
	   guid, logid, budget);
    
    ipc_env = idl4_default_environment;
    IEarm_Manager_budget_resource( earmmgr, guid, logid, budget, &ipc_env );


}


/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     microdisk_tsc/ebudget.c
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#define __USE_GNU	// for O_DIRECT
#include <fcntl.h>
#include <errno.h>

void usage( char *progname )
{
    fprintf( stderr,  "usage: %s <resource> <domain> <value>\n", progname );
    exit( 1 );
}

int main( int argc, char *argv[] )
{
    L4_ThreadId_t controller;
    L4_MsgTag_t tag;
    L4_Msg_t msg;
    L4_Word_t domain = 0;
    L4_Word_t resource = 0;
    L4_Word_t value = 0;

    if( argc < 3 )
	usage(argv[0]);
    
    controller.raw = 0x0011c002;
    resource = strtoul(argv[1], NULL, 0);
    domain = strtoul(argv[2], NULL, 0);
    value = strtoul(argv[3], NULL, 0);

    printf("Setting resource %d domain %d budget %d\n",
	   resource, domain, value);

    tag.raw = 0;
    tag.X.u = 3;

    L4_Set_MsgTag( tag );
    L4_LoadMR(1, resource);
    L4_LoadMR(2, domain);
    L4_LoadMR(3, value);
    
    L4_Send(controller);
}


/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers-init.c
 * Description:	This program installs the L4Ka driver modules.  It then
 * 		waits forever, because it runs as init, and thus
 * 		mustn't exit.
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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void wait_forever( void )
{
    int fds[2];
    char ch;

    pipe( fds );
    read( fds[0], &ch, sizeof(ch) );
}

int load_module( const char *module, int is_optional )
{
    static const char *insmod = "/insmod";
    int status;
    pid_t wait_pid, pid;
    struct stat stat_buf;

    if( stat(module, &stat_buf) != 0 )
    {
	    if (!is_optional)
		    printf("Mandatory module %s is missing\n", module);
	    return is_optional; // Module doesn't exist.
    }

    printf("Load module %s\n", module);
    pid = fork();
    if( pid == 0 ) {
	execl( insmod, insmod, module, NULL );
	puts( "Failure loading module: " );
	puts( module );
	exit( 1 );
    }

    wait_pid = waitpid( pid, &status, 0 );
    if( (wait_pid == pid) && WIFEXITED(status) && (0 == WEXITSTATUS(status)) )
	return 1;
    else
	return 0;
}

int execute( const char *name )
{
    int status;
    pid_t wait_pid, pid;

    pid = fork();
    if( pid == 0 ) {
	execl( name, name, NULL );
	puts( "Failure executing task: " );
	puts( name );
	exit( 1 );
    }

    wait_pid = waitpid( pid, &status, 0 );
    if( (wait_pid == pid) && WIFEXITED(status) && (0 == WEXITSTATUS(status)) )
	return 1;
    else
	return 0;
}

int main( int argc, char *argv[] )
{
    if( !load_module("/l4ka_glue.ko", 0) ||
	    !load_module("/l4ka_lanaddress.ko", 1) ||
	    !load_module("/l4ka_pci_server.ko", 1) ||
	    !load_module("/l4ka_net_server.ko", 1) ||
	    !load_module("/l4ka_block_server.ko", 1))
    {
	puts( "Initialization failure." );
    }

    if( !execute("/l4ka-vm-loaded") )
	puts( "Initialization failure." );

    if( getpid() == 1 ) {
	wait_forever();
	return 1;
    }

    return 0;
}


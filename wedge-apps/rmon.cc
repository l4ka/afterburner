#include <l4/types.h>
#include <l4/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <errno.h>
#include "resourcemon_idl_client.h"

void usage( char *progname )
{
    fprintf( stderr,  "usage: %s interval\n", progname );
    exit( 1 );
}

int main( int argc, char *argv[] )
{
    L4_ThreadId_t controller;
    L4_MsgTag_t tag;
    int value = 0;

    if( argc < 3 )
	usage(argv[0]);
    
    controller.raw = 0x0011c002;
    value = strtoul(argv[1], NULL, 0);

    printf("Setting migration interval %d\n", value);

    tag.raw = 0;
    tag.X.u = 3;

    L4_Set_MsgTag( tag );
    L4_LoadMR(1, value);
    
    L4_Send(controller);
}


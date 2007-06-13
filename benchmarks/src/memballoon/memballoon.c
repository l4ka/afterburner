
#include <stdio.h>
#include <stdlib.h>

#include <l4/types.h>
#include "hypervisor_idl_client.h"

void print_help(void) {
    printf("memballoon <tid> <vmid> <size KB>\n");
    exit(1);
}

int main( int argc, char *argv[] )
{
    L4_ThreadId_t tid;
    long vm, size;
    CORBA_Environment env = idl4_default_environment;

    if( argc != 4 )
	print_help();

    sscanf( argv[1], "%lx", &tid.raw );
    sscanf( argv[2], "%ld", &vm );
    sscanf( argv[3], "%ld", &size );
    printf( "control manager tid: 0x%lx\n", tid.raw );

    IVMControl_set_memballoon( tid, size * 1024U, vm, &env );

    if( env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &env );
	fprintf( stderr, "ipc failure to the working set scanner.\n" );
	exit( 1 );
    }

    exit(0);
}


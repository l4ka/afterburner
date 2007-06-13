
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


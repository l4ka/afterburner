
#include <stdio.h>
#include <stdlib.h>

#include <l4/types.h>
#include "hypervisor_idl_client.h"

int main( int argc, char *argv[] )
{
    L4_ThreadId_t tid;
    CORBA_Environment env = idl4_default_environment;
    L4_Word_t vm_id, milliseconds;

    if( argc != 4 ) {
	printf( "usage: %s <tid> <vm id> <milliseconds>\n", argv[0] );
	exit(1);
    }

    sscanf( argv[1], "%lx", &tid.raw );
    sscanf( argv[2], "%lu", &vm_id );
    sscanf( argv[3], "%lu", &milliseconds );

    IVMControl_start_active_page_scan( tid, milliseconds, vm_id, &env );

    if( env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &env );
	fprintf( stderr, "ipc failure to the working set scanner.\n" );
	exit( 1 );
    }

    exit(0);
}


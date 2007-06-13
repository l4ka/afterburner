/*********************************************************************
 *
 * Copyright (C) 2004 Karlsruhe University
 *
 * File path:	vm/benchmarks/set_io_ts/set_io_ts.c
 * Description:	L4Linux program to adjust the IOMMU timeslice of DDOSes
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: set_io_ts.c,v 1.2 2004/05/18 09:35:09 sgoetz Exp $
 *
 ********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <l4/types.h>
#include <asm-l4/mlx/hypervisor_idl_client.h>

L4_ThreadId_t tid_spaces[2] = { (L4_ThreadId_t){ raw: 0x8108002 },
				(L4_ThreadId_t){ raw: 0xc108002 } };
L4_ThreadId_t tid_hypervisor = (L4_ThreadId_t){ raw: 0x108001 };

int
main( int argc,
      char *argv[] )
{
    if( 3 == argc ) {
	int idx;

	for( idx = 0; idx < argc - 1; idx++ ) {
	    CORBA_Environment ipc_env;
	    long ts = strtol( argv[idx+1], NULL, 10 );
	    L4_ThreadId_t tid_dummy = L4_nilthread;

	    ipc_env = idl4_default_environment;
	    IHypervisor_SpaceControl( tid_hypervisor,
				      &tid_spaces[idx],
				      (1<<30 | L4_TimePeriod(ts * 1000).raw),
				      ~0UL, ~0UL, &tid_dummy, &ipc_env );

	    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
		printf( "Exception %d while setting IO time slice for space %d\n",
			ipc_env._minor, idx );
		return ipc_env._minor;
	    }
	}
    } else {
	printf("Usage: %s TS1 TS2\n  TS1/2:     time slice length for 1st/2nd DDOS in msecs\n",
	       argv[0] );
    }

    return 0;
}

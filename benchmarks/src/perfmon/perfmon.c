#include <stdio.h>
#include <stdlib.h>

#include <perfmon.h>

int main( int argc, char *argv[] )
{
    benchmarks_t *bm = NULL;
    enum bm_type_t bmt;

    bmt = ia32_cpu_type();
    switch (bmt) {
	case BMT_P4:
	    printf( "Pentium 4 detected, enabling P4 performance monitoring.\n" );
	    bm = &p4_benchmarks;
	    break;
	case BMT_K8:
	    printf( "AMD K8 detected, enabling K8 performance monitoring.\n" );
	    bm = &k8_benchmarks;
	    break;
	default:
	    printf( "Unknown CPU type, performance monitoring disabled\n" );
    }

    perfmon_start(bm);
    if( argc > 1 )
	system( argv[1] );
    perfmon_stop(bm);

    perfmon_print(bm);

    return 0;
}

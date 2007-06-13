#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <perfmon.h>

benchmarks_t *bm = NULL;
unsigned long long *samples;
unsigned next_sample = 0;
unsigned tot_samples = 0;
unsigned sample_width = 0;

void dumper( void )
{
    unsigned int i, sample;

    perfmon_print_headers(bm);

    for( sample = 0; sample < next_sample; sample++ )
    {
	printf( "%llu", samples[sample*sample_width+0] );
	for( i = 1; i < sample_width; i++ )
	    printf( ",%llu", samples[sample*sample_width+i] );
	printf( "\n" );
    }
}

void sigint_handler( int sig )
{
    dumper();
    exit( 1 );
}

int main( int argc, char *argv[] )
{
    enum bm_type_t bmt;
    unsigned total_time = 60, sample_period = 20;
    unsigned int i;

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

    if( argc > 1 )
	sscanf( argv[1], "%u", &sample_period );
    if( argc > 2 )
	sscanf( argv[2], "%u", &total_time );

    if( signal(SIGINT, sigint_handler) == SIG_ERR ) {
	printf( "Error installing the signal handler.\n" );
	exit( 1 );
    }

    tot_samples = total_time / sample_period;
    sample_width = bm->size + 1;
    samples = malloc( sizeof(unsigned long long) * sample_width * tot_samples );
    if( samples == NULL ) {
	printf( "Error allocating memory.\n" );
	exit( 1 );
    }

    for( next_sample = 0; next_sample < tot_samples; next_sample++ )
    {
	perfmon_start(bm);
	sleep( sample_period );
	perfmon_stop(bm);

	samples[next_sample*sample_width + 0] = bm->tsc;
	for( i = 0; i < bm->size; i++ )
	    samples[next_sample*sample_width + 1 + i] = bm->benchmarks[i].counter;
    }

    signal( SIGINT, SIG_DFL );
    dumper();

    return 0;
}

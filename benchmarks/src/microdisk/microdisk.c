/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     microdisk/microdisk.c
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#define __USE_GNU	// for O_DIRECT
#include <fcntl.h>
#include <errno.h>

#include "../../../hypervisor/include/ia32/perfmon.h"

#define MIN_BLOCK_SIZE	512
#define MAX_BLOCK_SIZE	(16*4096)
#define MAX_BLOCK_MASK	~(MAX_BLOCK_SIZE-1)
#define MAX_IOVEC	1024

char buffer_space[2*MAX_BLOCK_SIZE];

struct iovec iovec[MAX_IOVEC];

void usage( char *progname )
{
    fprintf( stderr,  "usage: %s <device name>\n", progname );
    exit( 1 );
}

int main( int argc, char *argv[] )
{
    int dev_fd;
    char *device_name;
    char *buffer = (char *)((unsigned long)(buffer_space + MAX_BLOCK_SIZE) & MAX_BLOCK_MASK);
    unsigned long device_size;
    unsigned block_size, block_count, block;
    int err, i;
    benchmarks_t *bm;

    if( argc < 2 )
	usage(argv[0]);
    device_name = argv[1];

    // Init the performance counters.
    bm = perfmon_arch_init();
    if( NULL == bm )
	exit( 1 );

    // Open the device with *raw* access.
    dev_fd = open( device_name, O_RDWR | O_DIRECT, 0 );
    if( dev_fd == -1 ) {
	fprintf( stderr, "Unable to open '%s': %s.\n",
		device_name, strerror(errno) );
	exit( 1 );
    }

    // Choose the number of blocks we want.
    device_size = 1UL*1024UL*1024UL*1024UL;
    block_count = device_size / (MAX_BLOCK_SIZE * MAX_IOVEC);

    // Init the iovec array.
    for( i = 0; i < MAX_IOVEC; i++ )
	iovec[i].iov_base = buffer;

    // Print the headers.
    printf( "type,syscalls,block_size,iovecs," );
    perfmon_print_headers(bm);
    fflush(stdout);

    // Perform the read experiment.
    for( block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size *= 2 )
    {
	if( lseek(dev_fd, 0, SEEK_SET) != 0 ) {
	    fprintf( stderr, "Seek error on '%s': %s.\n",
		    device_name, strerror(errno) );
	    exit( 1 );
	}

	for( i = 0; i < MAX_IOVEC; i++ )
	    iovec[i].iov_len = block_size;

	block_count = device_size / (block_size * MAX_IOVEC);

	printf( "read,%lu,%u,%u,", block_count, block_size, MAX_IOVEC );
	fflush(stdout);
	perfmon_start(bm);
	for( block = 0; block < block_count; block++ )
	{
    	    err = readv(dev_fd, iovec, MAX_IOVEC);
    	    if( err < 0 ) {
    		fprintf( stderr, "Read error on '%s': %s.\n",
    			device_name, strerror(errno) );
    		exit( 1 );
    	    }
       	}
	perfmon_stop(bm);
	perfmon_print_data(bm);
    }

    // Perform the write experiment.
    for( block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size *= 2 )
    {
	if( lseek(dev_fd, 0, SEEK_SET) != 0 ) {
	    fprintf( stderr, "Seek error on '%s': %s.\n",
		    device_name, strerror(errno) );
	    exit( 1 );
	}

	for( i = 0; i < MAX_IOVEC; i++ )
	    iovec[i].iov_len = block_size;

	block_count = device_size / (block_size * MAX_IOVEC);

	printf( "write,%lu,%u,%u,", block_count, block_size, MAX_IOVEC );
	fflush(stdout);
	perfmon_start(bm);
	for( block = 0; block < block_count; block++ )
	{
	    err = writev(dev_fd, iovec, MAX_IOVEC);
	    if( err < 0 ) {
		fprintf( stderr, "Write error on '%s': %s.\n",
			device_name, strerror(errno) );
		exit( 1 );
	    }
	}
	perfmon_stop(bm);
	perfmon_print_data(bm);
    }

    close( dev_fd );
}


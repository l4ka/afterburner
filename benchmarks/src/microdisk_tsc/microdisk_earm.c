/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     microdisk_tsc/microdisk_earm.c
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


#define MIN_BLOCK_SIZE	512
#define MAX_BLOCK_SIZE	(16*4096)
#define MAX_BLOCK_MASK	~(MAX_BLOCK_SIZE-1)
#define MAX_IOVEC	1024

char buffer_space[2*MAX_BLOCK_SIZE];

struct iovec iovec[MAX_IOVEC];

#define PREFIX "microdisk: "


void usage( char *progname )
{
    fprintf( stderr,  "usage: %s <device name> [block_size]\n", progname );
    exit( 1 );
}

typedef unsigned int __attribute__((__mode__(__DI__))) u64_t;

inline u64_t ia32_rdtsc(void)
{
    u64_t __return;

    __asm__ __volatile__ (
	"rdtsc"
	: "=A"(__return));

    return __return;
}

#define MHZ	(3000ULL)
#define TSC_TO_MSEC(cycles)	(((cycles)) / (MHZ * 1000ULL))
#define TSC_TO_SEC(cycles)	(((cycles)) / (MHZ * 1000ULL * 1000ULL))

char *default_debug_prefix = "microdisk";

int main( int argc, char *argv[] )
{
    int dev_fd;
    char *device_name;
    char *buffer = (char *)((unsigned long)(buffer_space + MAX_BLOCK_SIZE) & MAX_BLOCK_MASK);
    unsigned long device_size;
    unsigned block_size, block_count, block;
    unsigned min_block_size, max_block_size, user_block_size = 0;
    char *debug_name, *debug_prefix;
    FILE *debug_fd = stdout;	
    int err, i;
    u64_t last_time = 0, now_time = 0, delta_time = 0;
    long last_block = 0, delta_block = 0, mbs = 0;
    
    min_block_size = MIN_BLOCK_SIZE;
    max_block_size = MAX_BLOCK_SIZE;
    debug_prefix = default_debug_prefix;
    
    if (argc == 1)
    {
	device_name = "/dev/vmblock/0";
	min_block_size = max_block_size = 8192;
    }
    if (argc == 2)
    {
	device_name = argv[1];
    }
    else if (argc == 3)
    {
	device_name = argv[1];
	debug_name = argv[2];
	
	// Open the debug console.
	debug_fd = fopen( debug_name, "a+" );
	if( debug_fd == NULL ) {
	    fprintf( stderr, "Unable to open '%s': %s.\n",
		     device_name, strerror(errno) );
	    exit( 1 );
	}
    }
    else if (argc == 4)
    {
	device_name = argv[1];
	debug_name = argv[2];
	debug_prefix = argv[3];
	// Open the debug console.
	debug_fd = fopen( debug_name, "a+" );
	if( debug_fd == NULL ) {
	    fprintf( stderr, "Unable to open '%s': %s.\n",
		     device_name, strerror(errno) );
	    exit( 1 );
	}
    }
    else if (argc == 6)
    {
	device_name = argv[1];
	min_block_size = strtoul(argv[2], NULL, 0);
	max_block_size = strtoul(argv[3], NULL, 0);
	debug_name = argv[4];
	debug_prefix = argv[5];
	debug_fd = fopen( debug_name, "a+" );
	if( debug_fd == NULL ) {
	    fprintf( stderr, "Unable to open '%s': %s.\n",
		     device_name, strerror(errno) );
	    exit( 1 );
	}
    }


    fprintf(debug_fd, "%s Running microdisk benchmark on %s blocksizes %d to %d\n",
	    debug_prefix, device_name, min_block_size, max_block_size);


    // Open the device with *raw* access.
    dev_fd = open( device_name, O_RDWR | O_DIRECT, 0 );
    if( dev_fd == -1 ) {
	fprintf( stderr, "%s Unable to open '%s': %s.\n", debug_prefix,
		 device_name, strerror(errno) );
	exit( 1 );
    }


    // Choose the number of blocks we want.
    device_size = 2048UL*1024UL*1024UL;
    block_count = device_size / (MAX_BLOCK_SIZE * MAX_IOVEC);

    // Init the iovec array.
    for( i = 0; i < MAX_IOVEC; i++ )
	iovec[i].iov_base = buffer;

    // Print the headers.
    fflush(debug_fd);
	
    // Perform the read experiment.
    for( block_size = min_block_size; block_size <= max_block_size; block_size *= 2 )
    {
	if( lseek(dev_fd, 0, SEEK_SET) != 0 ) {
	    fprintf( debug_fd, "%s Seek error on '%s': %s.\n", 
		     debug_prefix, device_name, strerror(errno) );
	    exit( 1 );
	}

	for( i = 0; i < MAX_IOVEC; i++ )
	    iovec[i].iov_len = block_size;

	block_count = device_size / (block_size * MAX_IOVEC);

	    
	//fprintf(debug_fd, "%s read,%lu,%u,%u\n", block_count, block_size, MAX_IOVEC );
	fflush(debug_fd);
		

	for( block = 0; block < block_count; block++ )
	{
	    now_time = ia32_rdtsc();
	    err = readv(dev_fd, iovec, MAX_IOVEC);
	    if( err < 0 ) {
		fprintf( debug_fd, "%s Read error on '%s': %s.\n", 
			 debug_prefix, device_name, strerror(errno) );
		exit( 1 );
	    }
		
	    delta_time = now_time - last_time;
		
	    if (TSC_TO_MSEC(delta_time) > 1000)
	    {
		delta_block = block - last_block;
		mbs = delta_block * (block_size * MAX_IOVEC);
		mbs /= TSC_TO_MSEC(delta_time);
		fprintf(debug_fd, "%s read, %lu, %d MB/s\n", debug_prefix, block_size, mbs);
		last_time = now_time;
		last_block = block;
	    }
		
	}
	fflush(debug_fd);

	    
    }

    // Perform the write experiment.
    for( block_size = min_block_size; block_size <= max_block_size; block_size *= 2 )
    {
	if( lseek(dev_fd, 0, SEEK_SET) != 0 ) {
	    fprintf( debug_fd, "%s Seek error on '%s': %s.\n", 
		     debug_prefix, device_name, strerror(errno) );
	    exit( 1 );
	}

	for( i = 0; i < MAX_IOVEC; i++ )
	    iovec[i].iov_len = block_size;

	block_count = device_size / (block_size * MAX_IOVEC);

	fflush(debug_fd);
	for( block = 0; block < block_count; block++ )
	{
	    now_time = ia32_rdtsc();
	    err = writev(dev_fd, iovec, MAX_IOVEC);
	    if( err < 0 ) {
		fprintf( debug_fd, "%s Write error on '%s': %s.\n", 
			 debug_prefix, device_name, strerror(errno) );
		exit( 1 );
	    }
	    delta_time = now_time - last_time;
		
	    if (TSC_TO_MSEC(delta_time) > 1000)
	    {
		delta_block = block - last_block;
		mbs = delta_block * (block_size * MAX_IOVEC);
		mbs /= TSC_TO_MSEC(delta_time);
		fprintf(debug_fd, "%s write, %lu, %d MB/s\n", debug_prefix, block_size, mbs);
		last_time = now_time;
		last_block = block;
	    }

	}
    }

    close( dev_fd );
}


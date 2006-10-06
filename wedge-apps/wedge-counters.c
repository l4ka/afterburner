/*********************************************************************
 *                
 * Copyright (C) 2006,  University of Karlsruhe
 *                
 * File path:     wedge-apps/wedge-counters.c
 * Description:   Extract runtime profile counters from the wedge.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ia32/afterburn_syscalls.h"

typedef enum {false=0, true=1} bool;


word_t counters[65536 / sizeof(word_t)];

void usage( const char *progname )
{
    fprintf( stderr, "usage: %s [-k] [-z] [<filename>]\n",
	    progname );
    fprintf( stderr, 
	    "\t-k\tUse the kernel counters, rather than wedge counters.\n"
	    "\t-z\tZero the chosen counters, rather than save them.\n" );
    exit( 1 );
}

int main( int argc, const char *argv[] )
{
    int arg;
    bool do_kernel = false;
    bool do_zero = false;
    const char *filename = NULL;
    FILE *file = stdout;

    afterburn_err_e err;

    word_t i, result, start = 0;
    word_t cnt = sizeof(counters) / sizeof(word_t);

    for( arg = 1; arg < argc; arg++ ) {
	if( !strcmp(argv[arg], "-k") )
	    do_kernel = true;
	if( !strcmp(argv[arg], "-z") )
	    do_zero = true;
	else
	    filename = argv[arg];
    }

    if( do_zero ) {
	if( do_kernel )
	    afterburn_zero_kernel_counters( &err );
	else
	    afterburn_zero_wedge_counters( &err );
	return 0;
    }

    if( filename ) {
	file = fopen( filename, "w" );
	if( !file ) {
	    fprintf( stderr, "Unable to open output file '%s': %s",
		    filename, strerror(errno) );
	    exit( 1 );
	}
    }

    // Page-in the counters buffer.
    for( i = 0; i < cnt; i++ )
	counters[i] = 0;

    do {
	if( do_kernel )
	    result = afterburn_get_kernel_counters( start, counters, cnt, &err );
	else
	    result = afterburn_get_wedge_counters( start, counters, cnt, &err );
	if( err ) {
	    printf( "error: %u\n", err );
	    exit( 1 );
	}

	if( fwrite(counters, sizeof(word_t), result, file) != result ) {
	    fprintf( stderr, "Error writing to output file: %s\n",
		    strerror(errno) );
	    exit( 1 );
	}

	start += result;
    } while( result );


    if( filename )
	fclose( file );

    return 0;
}


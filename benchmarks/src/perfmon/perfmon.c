/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     perfmon/perfmon.c
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

/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/common/burn_counters.cc
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
 * $Id: burn_counters.cc,v 1.5 2006/01/03 15:23:49 joshua Exp $
 *
 ********************************************************************/
#include <burn_counters.h>

#include <console.h>

void burn_counters_dump()
    // Print and clear the counters.
{
    burn_counter_t *counter = burn_counter_t::get_start();
    burn_counter_t *end = burn_counter_t::get_end();

    while( counter < end ) {
	printf( counter->name << ' ' << counter->counter << '\n';
	counter->counter = 0;
	counter++;
    }

    burn_counter_region_t *region = burn_counter_region_t::get_start();
    burn_counter_region_t *region_end = burn_counter_region_t::get_end();

    while( region < region_end ) {
	printf( region->name << ":\n");
	for( word_t i = 0; i < region->word_cnt; i++ ) {
	    printf( "    " << i << ' ' << (void *)(i*4)
		<< ' ' << region->words[i] << '\n';
	    region->words[i] = 0;
	}
	region++;
    }

    perf_counters_dump();
}

void perf_counters_dump()
    // Print and clear the perf counters.
{
    perf_counter_t *counter = perf_counter_t::get_start();
    perf_counter_t *end = perf_counter_t::get_end();

    while( counter < end ) {
	printf( counter->name << ' ' << counter->counter << '\n';
	counter->counter = 0;
	counter++;
    }
}


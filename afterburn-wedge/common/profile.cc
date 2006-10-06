/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:     afterburn-wedge/common/profile.cc
 * Description:   Instruction profiler support.
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
 * $Id: profile.cc,v 1.4 2005/04/14 19:50:11 joshua Exp $
 *                
 ********************************************************************/

#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)

#include <profile.h>

instr_profiler_t instr_profiler;

void instr_profiler_t::add_patchups( patchup_info_t *patchups, word_t count,
	instr_group_t *group )
{
    for( word_t i = 0; i < count; i++ )
	add_instr( patchups[i].start, patchups[i].end, group );
}

void instr_profiler_t::add_instr( 
	word_t start_addr, word_t end_addr, instr_group_t *group )
{
    if( instr_cnt == max_instr )
	PANIC( "Too many instructions for the instruction profiler." );

    profile[instr_cnt].start_addr = start_addr;
    profile[instr_cnt].end_addr = end_addr;
    profile[instr_cnt].group = group;
    profile[instr_cnt].count = 0;

    instr_cnt++;
}

void instr_profiler_t::bubble_sort()
{
    for( word_t i = 0; i < instr_cnt-1; i++ ) {
	for( word_t j = i+1; j < instr_cnt; j++ ) {
	    if( profile[j].start_addr < profile[i].start_addr ) {
		instr_profile_t tmp = profile[j];
		profile[j] = profile[i];
		profile[i] = tmp;
	    }
	}
    }
}

void instr_profiler_t::count_return_addr( word_t addr )
    // Perform a binary search to locate the address.
{
    word_t start = 0, end = instr_cnt;
    word_t probe = (end - start) / 2;
    while( !profile[probe].return_intersect(addr) ) {
	word_t last_probe = probe;
	if( addr > profile[probe].start_addr )
	    start = probe;
	else
	    end = probe;
	probe = start + (end - start) / 2;
	if( probe == last_probe )
	    PANIC( "Unknown address for instruction profile, address " 
		    << (void *)addr << '.' );
    }

    profile[probe].count++;
}

void instr_profiler_t::dump_group( instr_group_t *group )
{
    con << "instruction profile for " << group->name << '\n';
    for( word_t i = 0; i < instr_cnt; i++ )
    {
	if( profile[i].group == group ) {
	    con << "    " << (void *)profile[i].start_addr 
		<< ' ' << profile[i].count << '\n';

	    profile[i].count = 0;
	}
    }
}


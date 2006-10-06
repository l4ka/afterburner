/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/profile.h
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
 * $Id: profile.h,v 1.2 2005/04/13 15:47:31 joshua Exp $
 *
 ********************************************************************/

#ifndef __AFTERBURN_WEDGE__INCLUDE__PROFILE_H__
#define __AFTERBURN_WEDGE__INCLUDE__PROFILE_H__

#include INC_ARCH(types.h)

#include <bind.h>

#if defined(CONFIG_INSTR_PROFILE)

#define ON_INSTR_PROFILE(a) a

struct instr_group_t {
    const char *name;
};

struct instr_profile_t {
    word_t start_addr;
    word_t end_addr;
    word_t count;
    instr_group_t *group;

    bool return_intersect( word_t addr )
	// Does the return address of a function call intersect with our
	// profile range?
    {
	return (addr > start_addr) && (addr <= end_addr);
    }
};

class instr_profiler_t {
public:
    static const word_t max_instr = 2048;

    void add_patchups( patchup_info_t *patchups, word_t count, instr_group_t *group );
    void add_instr( word_t start_addr, word_t end_addr, instr_group_t *group );
    void bubble_sort();
    void count_return_addr( word_t addr );

    void dump_group( instr_group_t *group );

    instr_profiler_t()
	{ instr_cnt = 0; }

    static instr_profiler_t & get_instr_profiler()
    {
	extern instr_profiler_t instr_profiler;
	return instr_profiler;
    }

private:
    instr_profile_t profile[max_instr];
    word_t instr_cnt;
};

INLINE void instr_profile_add_patchups( 
	patchup_info_t *patchups, word_t count, instr_group_t *group )
{
    instr_profiler_t::get_instr_profiler().add_patchups(patchups, count, group);
}

INLINE void instr_profile_return_addr( word_t return_addr )
{
    instr_profiler_t::get_instr_profiler().count_return_addr( return_addr );
}

INLINE void instr_profile_sort()
{
    instr_profiler_t::get_instr_profiler().bubble_sort();
}

INLINE void instr_profile_dump_group( instr_group_t *group )
{
    instr_profiler_t::get_instr_profiler().dump_group( group );
}

#else

#define ON_INSTR_PROFILE(a) do { } while(0)

#endif /* CONFIG_INSTR_PROFILE */

#endif	/* __AFTERBURN_WEDGE__INCLUDE__PROFILE_H__ */

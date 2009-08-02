/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/perf_counters.h
 * Description:   Declarations for burn counters.  Burn counters can
 *                be used for collecting various statistics.
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
 * $Id: perf_counters.h,v 1.1 2006/01/03 15:23:24 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__COMMON__PERF_COUNTERS_H__
#define __AFTERBURN_WEDGE__INCLUDE__COMMON__PERF_COUNTERS_H__

#if defined(CONFIG_BURN_COUNTERS)

#include INC_ARCH(types.h)
#include INC_ARCH(cycles.h)

extern void perf_counters_dump();

struct perf_counter_t {
    cycles_t counter;
    const char *name;

    static inline perf_counter_t *get_start()
    {
       	extern word_t _perf_counters_start[];
	return (perf_counter_t *)_perf_counters_start;
    }

    static inline perf_counter_t * get_end()
    {
       	extern word_t _perf_counters_end[];
	return (perf_counter_t *)_perf_counters_end;
    }
};

#define DECLARE_PERF_COUNTER(c)						\
    char * _counter_string__##c SECTION(".burn_strings") = MKSTR(c);	\
    perf_counter_t _counter__##c SECTION(".perf_counters") =		\
	{ 0, _counter_string__##c };

#define ADD_PERF_COUNTER(c,add)						\
    do {								\
	extern perf_counter_t _counter__##c; _counter__##c.counter += (add); \
    } while(0)


#else	/* Burn counters not configured. */

#define DECLARE_PERF_COUNTER(c)
#define ADD_PERF_COUNTER(c,add) do {} while(0)

#endif	/* CONFIG_BURN_COUNTERS */

#endif	/* __AFTERBURN_WEDGE__INCLUDE__COMMON__PERF_COUNTERS_H__ */

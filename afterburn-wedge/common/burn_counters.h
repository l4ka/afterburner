/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/burn_counters.h
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
 * $Id: burn_counters.h,v 1.6 2006/01/03 15:24:32 joshua Exp $
 *
 ********************************************************************/
#ifndef __COMMON__BURN_COUNTERS_H__
#define __COMMON__BURN_COUNTERS_H__

#include INC_ARCH(types.h)
#include <perf_counters.h>

#if defined(CONFIG_BURN_COUNTERS)

extern void burn_counters_dump();

struct burn_counter_t {
    word_t counter;
    const char *name;

    static inline burn_counter_t *get_start()
    {
       	extern word_t __burn_counters_start[];
	return (burn_counter_t *)__burn_counters_start;
    }

    static inline burn_counter_t * get_end()
    {
       	extern word_t __burn_counters_end[];
	return (burn_counter_t *)__burn_counters_end;
    }
};

#define DECLARE_BURN_COUNTER(c)						\
    char * __counter_string__##c SECTION(".burn_strings") = MKSTR(c);	\
    burn_counter_t __counter__##c SECTION(".burn_counters") =		\
	{ 0, __counter_string__##c };

#define INC_BURN_COUNTER(c)						\
    do { 								\
	extern burn_counter_t __counter__##c; __counter__##c.counter++;	\
    } while(0)

#define ADD_BURN_COUNTER(c,add)						\
    do {								\
	extern burn_counter_t __counter__##c; __counter__##c.counter += (add); \
    } while(0)
#define MAX_BURN_COUNTER(c,val)						\
    do {								\
	extern burn_counter_t __counter__##c; 				\
	if((val) > __counter__##c.counter) __counter__##c.counter=(val);\
    } while(0)


struct burn_counter_region_t {
    const char *name;
    word_t word_cnt;
    word_t *words;

    static inline burn_counter_region_t *get_start()
    {
       	extern word_t __burn_counter_regions_start[];
	return (burn_counter_region_t *)__burn_counter_regions_start;
    }

    static inline burn_counter_region_t *get_end()
    {
       	extern word_t __burn_counter_regions_end[];
	return (burn_counter_region_t *)__burn_counter_regions_end;
    }
};

#define DECLARE_BURN_COUNTER_REGION(c,word_cnt)				\
    char * __counter_string__##c SECTION(".burn_strings") = MKSTR(c);	\
    word_t __counter_region_words__##c [word_cnt] SECTION(".burn_regions"); \
    burn_counter_region_t __counter_region__##c 			\
	SECTION(".burn_counter_regions") =				\
	{ __counter_string__##c, word_cnt, __counter_region_words__##c };

#define INC_BURN_REGION_WORD(c, w)					\
    do { 								\
	extern burn_counter_region_t __counter_region__##c;		\
	__counter_region__##c.words[(w)]++;				\
    } while(0)

#define ON_BURN_COUNTER(a) a

#else	/* Burn counters not configured. */

#define DECLARE_BURN_COUNTER(c)
#define INC_BURN_COUNTER(c) do {} while(0)
#define ADD_BURN_COUNTER(c,add) do {} while(0)
#define MAX_BURN_COUNTER(c,add) do {} while(0)

#define DECLARE_BURN_COUNTER_REGION(c,word_cnt)
#define INC_BURN_REGION_WORD(c, w) do {} while(0)

#define ON_BURN_COUNTER(a)

#endif	/* CONFIG_BURN_COUNTERS */

#endif	/* __COMMON__BURN_COUNTERS_H__ */

/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     l4ka-debug.h
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
 ********************************************************************/
#ifndef __DEBUG_H__
#define __DEBUG_H__


#define L4_TRACEBUFFER

#if defined(cfg_cpu_p4)
#define L4_PERFMON
#define L4_CONFIG_CPU_X86_P4
#elif defined(cfg_cpu_k8)
#define L4_PERFMON
#define L4_CONFIG_CPU_X86_K8
#else
#undef L4_PERFMON
#endif

#if defined(pistachio_tbuf_energy) 
#if defined(cfg_cpu_p4) || defined(cfg_cpu_k8)
#define L4_PERFMON_ENERGY 
#endif
#endif


#include <l4/kdebug.h>
#include <l4/tracebuffer.h>

#define PREPAD		"             "
#define PREFIX		"\e[1m\e[33mresourcemon:\e[0m "
#define DEBUG_STATIC __attribute__((unused)) static  

#define DEBUG_TO_4CHAR(str)   (* (word_t *) str)		     

#define DEFAULT_DBG_LEVEL 3
#define DEFAULT_TRACE_LEVEL 6

extern L4_Bool_t l4_tracebuffer_enabled;
extern L4_Word_t dbg_level;
extern L4_Word_t trace_level;

void set_console_prefix(char* prefix);		

class debug_id_t
{
public:
    word_t id;
    word_t level;

    debug_id_t (word_t i, word_t l) { id = i; level = l; }

    inline debug_id_t operator + (const int &n) const
	{
	    return debug_id_t(id, level+n);
	}

    inline debug_id_t operator - (const int &n) const
	{
	    return debug_id_t(id, (level > (word_t) n ? level-n : 0));
	}

    inline operator bool (void) const
	{ 
	    return level;
	}


};


DEBUG_STATIC debug_id_t debug_startup           = debug_id_t( 1, 1);
DEBUG_STATIC debug_id_t debug_pfault		= debug_id_t( 5, 3);
DEBUG_STATIC debug_id_t debug_task 		= debug_id_t(13, 3);
DEBUG_STATIC debug_id_t debug_virq              = debug_id_t(34, 3);

DEBUG_STATIC debug_id_t debug_scheduler         = debug_id_t(54, 3);
DEBUG_STATIC debug_id_t debug_earm              = debug_id_t(55, 3);


extern "C" int trace_printf(debug_id_t id, const char* format, ...);	
extern "C" int l4kdb_printf(const char* format, ...);
extern "C" int dbg_printf(const char* format, ...);		

#if defined(cfg_optimize)
#define ASSERT(x)					
#define dprintf(...)
#else

#define dprintf(id,a...)					\
    do								\
    {								\
	if((id).level<dbg_level)				\
	    l4kdb_printf(a);					\
	if ((id).level<trace_level && l4_tracebuffer_enabled)	\
	    trace_printf(id, a, L4_TRACEBUFFER_MAGIC);		\
    } while(0)


extern void assert_hex_to_str( unsigned long val, char *s);
extern void assert_dec_to_str(unsigned long val, char *s);

#define ASSERT(x)					\
    do {						\
	if(EXPECT_FALSE(!(x))) {			\
	    extern char assert_string[512];		\
	    char *_d = &assert_string[11];		\
	    char *_s = NULL;				\
	    char _l[10];				\
	    assert_dec_to_str(__LINE__, _l);		\
	    _s = MKSTR(x);				\
	    while (*_s)	*_d++ = *_s++;			\
	    *_d++ = ' ';				\
	    _s = __FILE__;				\
	    while (*_s)	*_d++ = *_s++;			\
	    *_d++ = ' ';				\
	    _s = _l;					\
	    while (*_s)	*_d++ = *_s++;			\
	    *_d++ = '\n';				\
	    *_d++ = 0;					\
	    if (l4_tracebuffer_enabled)			\
		L4_Tbuf_RecordEvent (0, assert_string);	\
	    L4_KDB_PrintString(assert_string);		\
	    L4_KDB_Enter("panic");			\
	}						\
    } while(0)
#endif /* defined(cfg_optimize) */

#define  printf(x...)	dprintf(debug_id_t(0,0), x)
#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");

#endif	/* __DEBUG_H__ */

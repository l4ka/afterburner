/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/vm.cc
 * Description:   The L4 state machine for implementing the idle loop,
 *                and the concept of "returning to user".  When returning
 *                to user, enters a dispatch loop, for handling L4 messages.
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
#include <console.h>
#include INC_WEDGE(sync.h)
#include INC_WEDGE(user.h)

word_t irq_traced = 0;
word_t vector_traced[8];

char assert_string[512] = "ASSERTION:  ";
void debug_hex_to_str( unsigned long val, char *s )
{
    static const char representation[] = "0123456789abcdef";
    bool started = false;
    for( int nibble = 2*sizeof(val) - 1; nibble >= 0; nibble-- )
    {
	unsigned data = (val >> (4*nibble)) & 0xf;
	if( !started && !data )
	    continue;
	started = true;
	*s++ = representation[data] ;
    }
}

void debug_dec_to_str(unsigned long val, char *s)
{
    L4_Word_t divisor;
    int width = 8, digits = 0;

    /* estimate number of spaces and digits */
    for (divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++);

    /* print spaces */
    for ( ; digits < width; digits++ )
	*s++ = ' ';

    /* print digits */
    do {
	*s++ = (((val/divisor) % 10) + '0');
    } while (divisor /= 10);

}

#if defined(L4KA_ASSERT_SYNC)
char lock_assert_string[] = "LOCK_ASSERT(x, LCKN)";
#endif

void mr_save_t::dump(word_t level)
{
    dprintf(level, "tag %08x eip %08x efl %08x edi %08x esi %08x ebp %08x esp %08x ",
	    tag.raw, get(OFS_MR_SAVE_EIP), get(OFS_MR_SAVE_EFLAGS), get(OFS_MR_SAVE_EDI),
	   get(OFS_MR_SAVE_ESI), get(OFS_MR_SAVE_EBP), get(OFS_MR_SAVE_ESP)); 
    dprintf(level, "ebx %08x edx %08x ecx %08x eax %08x\n", get(OFS_MR_SAVE_EBX),	 
	   get(OFS_MR_SAVE_EDX), get(OFS_MR_SAVE_ECX), get(OFS_MR_SAVE_EAX));
   
}

hiostream_kdebug_t::buffer_t hiostream_kdebug_t::buffer[hiostream_kdebug_t::max_clients];
word_t hiostream_kdebug_t::clients = 0;
cpu_lock_t hiostream_kdebug_t::lock;
bool hiostream_kdebug_t::initialized;
bool hiostream_kdebug_t::single_user = true;
IConsole_handle_t hiostream_kdebug_t::handle;
IConsole_content_t hiostream_kdebug_t::content;
CORBA_Environment hiostream_kdebug_t::env;

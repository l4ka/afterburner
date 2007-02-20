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
#include INC_WEDGE(console.h)
#include INC_WEDGE(sync.h)
#include INC_WEDGE(user.h)

#if defined(L4KA_DEBUG_SYNC)
char lock_debug_string[] = "LOCK_DBG: C LCKN 12345678 1 12345678 1\n";
#endif
#if defined(L4KA_ASSERT_SYNC)
char lock_assert_string[] = "LOCK_ASSERT(x, LCKN)";
#endif

void mr_save_t::dump()
{
    con
	<< "eip "   << (void *) get(OFS_MR_SAVE_EIP)	
	<< " efl "  << (void *) get(OFS_MR_SAVE_EFLAGS)
	<< " edi "  << (void *) get(OFS_MR_SAVE_EDI)
	<< " esi "  << (void *) get(OFS_MR_SAVE_ESI)
	<< " ebp"   << (void *) get(OFS_MR_SAVE_EBP)
	<< "\nesp " << (void *) get(OFS_MR_SAVE_ESP)
	<< " ebx "  << (void *) get(OFS_MR_SAVE_EBX)
	<< " edx "  << (void *) get(OFS_MR_SAVE_EDX)
	<< " ecx "  << (void *) get(OFS_MR_SAVE_ECX)
	<< " eax "  << (void *) get(OFS_MR_SAVE_EAX)
	<< "\n";
}

hiostream_kdebug_t::buffer_t hiostream_kdebug_t::buffer[hiostream_kdebug_t::max_clients];
int hiostream_kdebug_t::clients = 0;
IConsole_handle_t hiostream_kdebug_t::handle;
IConsole_content_t hiostream_kdebug_t::content;
CORBA_Environment hiostream_kdebug_t::env;

/*********************************************************************
 *                
 * Copyright (C) 2006, University of Karlsruhe
 *                
 * File path:     afterburn-wedge/common/wedge_syscalls.cc
 * Description:   Infrastructure for system calls from the guest apps
 *                to the wedge.
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


#include INC_ARCH(wedge_syscalls.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(backend.h)


/*
 * x86 wedge system calls:
 *
 * The user program identifies the target syscall with an ID in eax.
 * The syscall entry path changes eax into a pointer to the interrupt frame.
 * Additional user parameters reside in edx and ecx.  A return value 
 * is provided in eax.  On error, the return value is an error code.
 * On success, the return instruction pointer is incremented by 5, to
 * jump over the error-handling logic.
 */

static bool copy_to_user( word_t *src, word_t cnt, word_t user_addr )
{
    word_t kernel_addr;

    // We copy using kernel addresses, because they are valid in
    // any wedge, while user addresses are wedge dependent.
    pgent_t *pgent = backend_resolve_addr( user_addr, kernel_addr );
    if( !pgent || !pgent->is_writable() )
	return false;

    for( word_t i = 0; i < cnt; i++ )
    {
	*(word_t *)kernel_addr = *src;

	kernel_addr += sizeof(word_t);
	user_addr += sizeof(word_t);
	src++;

	if( (user_addr & PAGE_MASK) == user_addr ) {
	    // At a page boundary; determine a new translation.
	    pgent = backend_resolve_addr( user_addr, kernel_addr );
	    if( !pgent || !pgent->is_writable() )
		return false;
	}
    }

    return true;
}

word_t __attribute__((regparm(3)))
wedge_syscall_get_counters( wedge_syscall_frame_t *frame,
	word_t start, word_t user_addr, word_t count )
{
#if defined(CONFIG_BURN_PROFILE)
    extern word_t _burn_profile_start[], _burn_profile_end[];
    word_t total = _burn_profile_end - _burn_profile_start;
    // Count, start, and total are indices into the array; not byte offsets.
    if( count + start > total )
	count = total - start;

    if( !copy_to_user(&_burn_profile_start[start], count, user_addr) ) {
	frame->commit_error();
	return invalid_user_addr;
    }
#else
    count = 0;
#endif

    frame->commit_success();
    return count;
}


word_t __attribute__((regparm(1)))
wedge_syscall_zero_counters( wedge_syscall_frame_t *frame )
{
#if defined(CONFIG_BURN_PROFILE)
    extern word_t _burn_profile_start[], _burn_profile_end[];
    word_t *counter;
    for( counter = _burn_profile_start; counter < _burn_profile_end; counter++ )
	*counter = 0;
#endif
    frame->commit_success();
    return 0;
}


word_t __attribute__((regparm(3)))
wedge_syscall_get_guest_counters( wedge_syscall_frame_t *frame,
	word_t start, word_t user_addr, word_t count )
{
    extern word_t *guest_burn_prof_counters_start;
    extern word_t *guest_burn_prof_counters_end;

    if( !guest_burn_prof_counters_start || !guest_burn_prof_counters_end ) {
	frame->commit_success();
	return 0;
    }

    word_t total = guest_burn_prof_counters_end - guest_burn_prof_counters_start;
    // Count, start, and total are indices into the array; not byte offsets.
    if( count + start > total )
	count = total - start;

    if( !copy_to_user(&guest_burn_prof_counters_start[start], count, user_addr) ) {
	frame->commit_error();
	return invalid_user_addr;
    }

    frame->commit_success();
    return count;
}


word_t __attribute__((regparm(1)))
wedge_syscall_zero_guest_counters( wedge_syscall_frame_t *frame )
{
    extern word_t *guest_burn_prof_counters_start;
    extern word_t *guest_burn_prof_counters_end;
    word_t *counter;

    for( counter = guest_burn_prof_counters_start;
	    counter < guest_burn_prof_counters_end; counter++ )
	*counter = 0;

    frame->commit_success();
    return 0;
}


wedge_syscall_t wedge_syscall_table[] = {
    (wedge_syscall_t)wedge_syscall_get_counters,
    (wedge_syscall_t)wedge_syscall_get_guest_counters,
    (wedge_syscall_t)wedge_syscall_zero_counters,
    (wedge_syscall_t)wedge_syscall_zero_guest_counters,
};

word_t max_wedge_syscall = 
    sizeof(wedge_syscall_table)/sizeof(wedge_syscall_t);


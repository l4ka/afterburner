/*********************************************************************
 *
 * Copyright (C) 2006,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/ia32/wedge_syscalls.h
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
#ifndef __IA32__WEDGE_SYSCALLS_H__
#define __IA32__WEDGE_SYSCALLS_H__

#include INC_ARCH(cpu.h)

class wedge_syscall_frame_t {
public:
    iret_user_frame_t iret;

    void commit_error() { }
    void commit_success() {
	this->iret.ip += 6;
    }
};

enum wedge_syscall_err_e { invalid_user_addr=1, illegal_syscall=2 };

typedef word_t (*wedge_syscall_t)( wedge_syscall_frame_t * );

extern "C" wedge_syscall_t wedge_syscall_table[];
extern "C" word_t max_wedge_syscall;

#endif	/* __IA32__WEDGE_SYSCALLS_H__ */

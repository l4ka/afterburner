/*********************************************************************
 *                
 * Copyright (C) 2005-2008,  University of Karlsruhe
 *                
 * File path:     console.h
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__CONSOLE_H__
#define __AFTERBURN_WEDGE__INCLUDE__CONSOLE_H__

#include <debug.h>
#include INC_WEDGE(console.h)

#define  printf(x...)	dprintf(debug_id_t(0,0), x)

typedef void (*console_putc_t)(const char c);
typedef void (*console_commit_t)();
extern console_putc_t console_putc;

extern void console_init( console_putc_t putc, const char *prefix=NULL, const bool do_vprefix=true,
			  console_commit_t commit=NULL);

#endif	/* __AFTERBURN_WEDGE__INCLUDE__CONSOLE_H__ */

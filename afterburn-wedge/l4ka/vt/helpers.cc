
/*********************************************************************
 *
 * Copyright (C) 2002, 2005-2004,  Karlsruhe University
 *
 * File path:     testvirt/helpers.cc
 * Description:   Testing Pacifica Extensions of L4 application
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
 * $Id: pingpong.cc,v 1.56 2005/07/14 17:50:31 ud3 Exp $
 *
 ********************************************************************/

#include <l4/types.h>
#include <l4/ipc.h>

#if defined(L4_ARCH_AMD64)
# include INC_WEDGE(vt/amd64.h)
#elif defined(L4_ARCH_IA32)
# include INC_WEDGE(vt/ia32.h)
#else
#warning ARCH not defined!
#endif


L4_Word_t roundUpNextLog2Sz (L4_Word_t sz)
{
    L4_Word8_t i = 0;
    while (sz != 0)
    {
	sz = (sz >> 1);
	i++;
    }

    return (1UL << i);
}


L4_Word_t L4_IsInFpage (const L4_Fpage_t f, const L4_Word_t address)
{
    return (L4_Address (f) >> L4_SizeLog2 (f)) == (address >> L4_SizeLog2 (f));
}

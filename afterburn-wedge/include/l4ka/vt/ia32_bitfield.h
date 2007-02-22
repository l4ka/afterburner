/*********************************************************************
 *
 * Copyright (C) 2006,  Karlsruhe University
 *
 * File path:     testvirt/ia32_bitfield.h
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
 * $Id:$
 *
 ********************************************************************/
#ifndef __TESTVIRT__IA32_BITFIELD_H__
#define __TESTVIRT__IA32_BITFIELD_H__

template<u32_t size>
INLINE void bitfield_t<size>::setBit (L4_Word_t index)
{
    ASSERT(this->isIn (index));

    __asm__ __volatile__ ("bts %0, %1  \t\n"
			  :
			  : "Ir" (index), "m" (this->raw[0]));
    return;
}


template<u32_t size>
INLINE void bitfield_t<size>::unsetBit (L4_Word_t index)
{
    __asm__ __volatile__ ("btr %0, %1  \t\n"
			  :
			  : "Ir"(index), "m" (this->raw[0]));
    return;
}


template<u32_t size>
INLINE bool bitfield_t<size>::isSet (L4_Word_t index) const
{
    ASSERT(this->isIn(index));

    char retval;

    __asm__ __volatile__ ("bt %1, %2    \t\n"
			  "setc %0      \t\n"
			  : "=rm" (retval)
			  : "Ir" (index), "m" (this->raw[0]));
    return retval;
}


#endif /* !__TESTVIRT__IA32_BITFIELD_H__ */

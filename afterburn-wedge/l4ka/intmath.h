/*********************************************************************
 *                
 * Copyright (C) 2007,  University of Karlsruhe
 *                
 * File path:     math.h
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

#ifndef __INTMATH_H__
#define __INTMATH_H__

/* compute with 64 bit intermediate result: (a*b)/c */
static inline u32_t muldiv32(u32_t a, u32_t b, u32_t c)
{
    u64_t n = (u64_t) a * b;
    if (c == 0) 
	return 0;
    
    n /= c;
    
    return n;
}


/* From QEMU System Emulator vl.c 
 * compute with 96 bit intermedia result: (a*b)/c */
static inline u64_t muldiv64(u64_t a, u32_t b, u32_t c)
{
    union {
        u64_t ll;
        struct {
#ifdef WORDS_BIGENDIAN
            u32_t high, low;
#else
            u32_t low, high;
#endif
        } l;
    } u, res;
    u64_t rl, rh;

    u.ll = a;
    rl = (u64_t)u.l.low * (u64_t)b;
    rh = (u64_t)u.l.high * (u64_t)b;
    rh += (rl >> 32);
    res.l.high = rh / c;
    res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
    return res.ll;

}

#endif	/* __INTMATH_H__ */

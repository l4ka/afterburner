/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 * Copyright (C) 2007,  Tom Bachmann
 *
 * File path:     afterburn-wedge/include/bitfield.h
 * Description:   Macros to declare wordsize-dependent bitfields.
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

#ifndef __AFTERBURN_WEDGE__INCLUDE__BITFIELD_H__
#define __AFTERBURN_WEDGE__INCLUDE__BITFIELD_H__

#include INC_ARCH(config.h)

#if CONFIG_BITWIDTH == 32
#define BITFIELD_32_64(n, m) : n
#define BITFIELD_64(n) : 0
#elif CONFIG_BITWIDTH == 64
#define BITFIELD_32_64(n, m) : m
#define BITFIELD_64(n) : n
#else
#error "Not ported to this bitwidth!"
#endif

#endif  /* __AFTERBURN_WEDGE__INCLUDE__BITFIELD_H__ */

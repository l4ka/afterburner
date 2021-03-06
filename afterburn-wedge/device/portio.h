/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/portio.h
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
 * $Id: portio.h,v 1.7 2005/11/07 16:55:05 joshua Exp $
 *
 ********************************************************************/
#ifndef __DEVICE__PORTIO_H__
#define __DEVICE__PORTIO_H__

#include INC_ARCH(cpu.h)

extern bool portio_read( u16_t port, u32_t & value, u32_t bit_width );
extern bool portio_write( u16_t port, u32_t value, u32_t bit_width );
extern bool portio_string_read(word_t port, word_t mem, word_t count, word_t bit_width, bool df);
extern bool portio_string_write(word_t port, word_t mem, word_t count, word_t bit_width, bool df);

extern void i8042_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void i8259a_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void i8253_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void i8254_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void mc146818rtc_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void serial8250_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void legacy_0x61( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void ide_portio( u16_t port, u32_t & value, u32_t bit_width, bool read );
extern void i82371ab_portio(u16_t port, u32_t & value, u32_t bit_width, bool read);
extern void pci_portio(u16_t port, u32_t & value, u32_t bit_width, bool read);

extern void serial8250_receive_byte( u8_t byte, word_t which=0 );

#endif	/* __DEVICE__PORTIO_H__ */

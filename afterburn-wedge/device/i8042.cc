/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/i8042.cc
 * Description:   Keyboard emulation.
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
 * $Id: i8042.cc,v 1.1 2005/11/07 16:42:17 joshua Exp $
 *
 ********************************************************************/


#include <device/portio.h>
#include INC_WEDGE(console.h)
#include INC_WEDGE(backend.h)

static const word_t i8042_status_port = 0x64;
static const word_t i8042_command_port = 0x64;
static const word_t i8042_data_reg = 0x60;

class i8042_status_t {
public:
    union {
	u8_t raw;
	struct {
	    u8_t obf : 1;
	    u8_t ibf : 1;
	    u8_t muxerr : 1;
	    u8_t cmddat : 1;
	    u8_t keylock : 1;
	    u8_t auxdata : 1;
	    u8_t timeout : 1;
	    u8_t parity : 1;
	} fields;
    } x;
};

class i8042_command_t {
public:
    union {
	u8_t raw;
	struct {
	    u8_t kbdint : 1;
	    u8_t auxint : 1;
	    u8_t yoda : 1;
	    u8_t ignkeylock : 1;
	    u8_t kbddis : 1;
	    u8_t auxdis : 1;
	    u8_t xlate : 1;
	    u8_t vader : 1;
	} fields;
    } x;
};


void i8042( u16_t port, u32_t &value, bool read )
{
    if( read ) {
	if( port == i8042_status_port ) {
	    i8042_status_t status;
	    status.x.raw = 0;
	    status.x.fields.ibf = 1;
	    value = status.x.raw;
	}
	return;
    }

    if( (port == i8042_command_port) && (value == 0xfe) ) {
	backend_reboot();
	return;
    }
}


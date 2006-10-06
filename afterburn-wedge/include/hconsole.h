/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/hconsole.h
 * Description:   The declaration of the hconsole_t type.
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
 * $Id: hconsole.h,v 1.2 2005/04/13 15:47:31 joshua Exp $
 *
 ********************************************************************/

#ifndef __AFTERBURN_WEDGE__INCLUDE__HCONSOLE_H__
#define __AFTERBURN_WEDGE__INCLUDE__HCONSOLE_H__

#include <hiostream.h>

class hconsole_t : public hiostream_t
{
private:
    const char *prefix;
    bool do_prefix;

protected:
    virtual void print_char( char ch );

public:
    virtual void init( hiostream_driver_t *new_driver, const char *new_prefix )
    {
	hiostream_t::init( new_driver );
	this->do_prefix = true;
	this->prefix = new_prefix;
    }
};

#endif	/* __AFTERBURN_WEDGE__INCLUDE__HCONSOLE_H__ */

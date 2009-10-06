/*********************************************************************
 *
 * Copyright (C) 2005,  National ICT Australia (NICTA)
 *
 * File path:     afterburn-wedge/kaxen/iostream.h
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
 * $Id: iostream.h,v 1.2 2005/11/04 16:00:30 joshua Exp $
 *
 ********************************************************************/
#ifndef __KAXEN__HIOSTREAM_H__
#define __KAXEN__HIOSTREAM_H__

#include <hiostream.h>
#include INC_WEDGE(xen_hypervisor.h)
#include INC_WEDGE(controller.h)

class hiostream_kaxen_t : public hiostream_driver_t
{
public:
    virtual void print_char(char ch)
    { 
	if( xen_start_info.is_privileged_dom() )
	    XEN_console_io(CONSOLEIO_write, 1, &ch);
	else {
	    xen_controller.console_write( ch );
	    if( ch == '\n' )
		xen_controller.console_write( '\r' );
	}
    }

    virtual char get_blocking_char()
    {
	if( xen_start_info.is_privileged_dom() )
	{
	    char key;
	    while( XEN_console_io(CONSOLEIO_read, 1, &key) != 1 )
		;
	    return key;
	}
	else
	    return xen_controller.console_destructive_read();
    }
};


#endif	/* __KAXEN__HIOSTREAM_H__ */

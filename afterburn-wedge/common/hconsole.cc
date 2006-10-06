/*********************************************************************
 *                
 * Copyright (C) 2005-2006,  University of Karlsruhe
 *                
 * File path:     hconsole.cc
 * Description:   Implements a crude console for the wedge, and 
 *                which prints a colored prefix before each line.
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
 * $Id: hconsole.cc,v 1.2 2005/04/13 11:09:53 joshua Exp $
 *                
 ********************************************************************/

#include <hconsole.h>

void hconsole_t::print_char( char ch )
{
    // If at beginning of the line, then print the prefix.
    if( this->do_prefix && this->prefix )
    {
	// Save the current color settings, and then switch to
	// our prefix colors.
	hiostream_driver_t::io_color_e color, background;
	color = this->get_color();
	background = this->get_background();
	this->reset_attributes();
	this->set_color( hiostream_driver_t::white );
	this->set_background( hiostream_driver_t::blue );

	const char *p = this->prefix;
	while( *p ) 
	    hiostream_t::print_char( *p++ );
	this->do_prefix = false;

	// Restore the original color settings.
	this->reset_attributes();
	this->set_color( color );
	this->set_background( background );
    }
    // Finally print the character.
    hiostream_t::print_char( ch );
    // Detect line ending.
    if( ch == '\n' )
	this->do_prefix = true;
}


/*********************************************************************
 *                
 * Copyright (C) 2005-2006, 2008,  University of Karlsruhe
 *                
 * File path:     hiostream.cc
 * Description:   Implementations for the console iostream.
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
 * $Id: hiostream.cc,v 1.5 2006/02/10 14:19:11 store_mrs Exp $
 *                
 ********************************************************************/

#include <hiostream.h>
#include <memory.h>

char color_escape[7] = "\e[37m";

void hiostream_driver_t::print_color_escape( io_color_e out_color, char base )
{
    color_escape[2] = base ;
    color_escape[3] = out_color + '0';
    this->print_char(color_escape[0]); 
    this->print_char(color_escape[1]); 
    this->print_char(color_escape[2]); 
    this->print_char(color_escape[3]); 
    this->print_char(color_escape[4]); 
    this->print_char(color_escape[5]); 
    this->print_char(color_escape[6]); 
}

void hiostream_driver_t::print_attribute( char attr )
{
    this->print_char( 27 );
    this->print_char( '[' );
    this->print_char( attr );
    this->print_char( 'm' );
}


void hiostream_t::print_string( const char *s )
{
   int w = this->width - strlen(s);
   if (w > 0) {
       for (int i = 0; i < w; i++)
	   this->print_char( ' ' );
   }
   while( *s )
       this->print_char( *s++ );
}


void hiostream_t::print_word( unsigned long val )
{
    unsigned long divisor;
    int digits;

    /* Estimate number of digits. */
    for( divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++ );
    
    /* Print digits. */
    do {
	this->print_char( ((val/divisor) % 10) + '0' );
    } while( divisor /= 10 );
}

void hiostream_t::print_long_long( unsigned long long val )
{
    unsigned long long divisor;
    int digits;

    /* Estimate number of digits. */
    for( divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++ );
    /* Print digits. */
    do {
	this->print_char( ((val/divisor) % 10) + '0' );
    } while( divisor /= 10 );
}

void hiostream_t::print_double( double val )
{
    unsigned long word;

    if( val < 0.0 )
    {
	this->print_char( '-' );
	val *= -1.0;
    }

    word = (unsigned long)val;
    this->print_word( word );
    val -= (double)word;
    if( val <= 0.0 )
	return;

    this->print_char( '.' );
    for( int i = 0; i < 3; i++ )
    {
	val *= 10.0;
	word = (unsigned long)val;
	this->print_word(word);
	val -= (double)word;
    }
}

void hiostream_t::print_hex( unsigned long val )
{
    static char representation[] = "0123456789abcdef";
    bool started = false;
    for( int nibble = 2*sizeof(val) - 1; nibble >= 0; nibble-- )
    {
	unsigned data = (val >> (4*nibble)) & 0xf;
	if( !started && !data )
	    continue;
	started = true;
	this->print_char( representation[data] );
    }
}

void hiostream_t::print_hex( void *val )
{
    static char representation[] = "0123456789abcdef";
    unsigned char *bytes = (unsigned char *)&val;
    for( int byte = sizeof(val)-1; byte >= 0; byte-- )
    {
	this->print_char( representation[(bytes[byte] >> 4) & 0xf] );
	this->print_char( representation[(bytes[byte]) & 0xf] );
    }
}

hiostream_t& hiostream_t::operator<< (void *p)
{
    // Print a pointer representation.
    this->print_char('0'); this->print_char('x');
    this->print_hex( p );
    return *this;
}

hiostream_t& hiostream_t::operator<< (long val)
{
    if( val < 0 )
    {
	this->print_char( '-' );
	val = -val;
    }
    print_word( val );
    return *this;
}

hiostream_t& hiostream_t::operator<< (long long val)
{
    if( val < 0 )
    {
	this->print_char( '-' );
	val = -val;
    }
    print_long_long( val );
    return *this;
}

#if defined(CONFIG_WEDGE_L4KA)
hiostream_t& hiostream_t::operator<< (L4_ThreadId_t tid)
{
    this->print_char( '0' ); this->print_char( 'x' );
    this->print_hex( (void *)tid.raw );
    if( L4_IsGlobalId(tid) ) {
	this->print_char( ' ' ); this->print_char( '<' );
	this->print_hex( L4_ThreadNo(tid) );
	this->print_char( ':' );
	this->print_hex( L4_Version(tid) );
	this->print_char( '>' );
    }
    return *this;
}
#endif


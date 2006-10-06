/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/hconsole.cc
 * Description:   Implements a console for the resourcemon.
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

#include <common/hconsole.h>

hconsole_t hout;

void hiostream_ansi_t::print_color_escape( io_color_e out_color, char base )
{
    this->print_char( 27 );
    this->print_char( '[' );
    this->print_char( base );
    this->print_char( out_color + '0' );
    this->print_char( 'm' );
}

void hiostream_ansi_t::print_attribute( char attr )
{
    this->print_char( 27 );
    this->print_char( '[' );
    this->print_char( attr );
    this->print_char( 'm' );
}


void hiostream_t::print_string( const char *s )
{
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

#if !defined(HCONSOLE_TYPE_SIMPLE)
hiostream_t& hiostream_t::operator<< (L4_ThreadId_t tid)
{
    this->print_char( '0' ); this->print_char( 'x' );
    this->print_hex( (void *)tid.raw );
    this->print_char( ' ' ); this->print_char( '<' );
    this->print_hex( L4_ThreadNo(tid) );
    this->print_char( ':' );
    this->print_hex( L4_Version(tid) );
    this->print_char( '>' );
    return *this;
}
#endif

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


#if defined(POSIX_TEST)

#include <stdio.h>

class hiostream_posix_t : public hiostream_ansi_t
{
public:
    virtual void print_char( char ch )
	{ putchar(ch); }

    virtual char get_blocking_char()
	{ return getchar(); }
};

int main( void )
{
    hiostream_posix_t con_driver;

    con_driver.init();
    hout.init( &con_driver, "resourcemon: " );

    hout << "yoda\n" << "was\n" << "here" << " a " << "while" << " ago\n";
    hout << (int *)3333 << '\n';
    hout << 5 << '\n';
    hout << (long)10 << '\n';
    hout << -3333 << '\n';
    hout << (long)-5555 << '\n';
    hout << (unsigned long)-5555 << '\n';
    hout << (unsigned long)5555 << '\n';
    hout << (unsigned)5678 << '\n';
    hout << (short)-134 << '\n';
    hout << (unsigned short)567 << '\n';
    hout << "long long:" << (long long)-4324 << '\n';
    hout << "unsigned long long:" << (unsigned long long)-2 << '\n';
    hout << 'c' << 'a' << 't' << '\n';
    hout.set_color(hiostream_driver_t::yellow);
    hout << "hah\n";
    hout.reset_attributes();
    hout << "vader\n";
    hout << (float)45.2389 << '\n';
    hout << 45.623 << '\n';

    char a, b, c;
    hout >> a >> b >> c;
    hout << a << b << c << '\n';
}

#endif /* POSIX_TEST */


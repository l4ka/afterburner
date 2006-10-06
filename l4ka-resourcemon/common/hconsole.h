/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/hconsole.h
 * Description:   Declarations for the resourcemon's console.
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
#ifndef __HYPERVISOR__INCLUDE__CONSOLE_H__
#define __HYPERVISOR__INCLUDE__CONSOLE_H__

#if !defined(POSIX_TEST)
#include <l4/ipc.h>
#if defined(cfg_console_kdebug)
#include <l4/kdebug.h>
#endif
#endif

class hiostream_driver_t
{
public:
    enum io_color_e {
	unknown=-1, black=0, red, green, yellow, blue, magenta, cyan, white
    };

protected:
    io_color_e color;
    io_color_e background;

public:
    friend class hiostream_t;

    virtual void init()
	{ this->color = this->background = unknown; }

    // I/O functions.
    virtual void print_char( char ch ) = 0;
    virtual char get_blocking_char() = 0;

    // Color functions.
    virtual void set_color( io_color_e new_color )
	{ this->color = new_color; }
    virtual void set_background( io_color_e new_color )
	{ this->background = new_color; }
    virtual void reset_attributes()
	{ this->background = this->color = unknown; }

    io_color_e get_color()		{ return this->color; }
    io_color_e get_background()		{ return this->background; }
};

class hiostream_ansi_t : public hiostream_driver_t
{
protected:
    void print_color_escape( io_color_e color, char base );
    void print_attribute( char attr );

public:
    virtual void set_color( io_color_e new_color )
    { 
	hiostream_driver_t::set_color( new_color );
	if( new_color != unknown )
	    print_color_escape( new_color, '3' ); 
    }

    virtual void set_background( io_color_e new_color )
    {
	hiostream_driver_t::set_background( new_color );
	if( new_color != unknown )
	    print_color_escape( new_color, '4' ); 
    }

    virtual void set_bold()
	{ print_attribute( '1' ); }

    virtual void reset_attributes()
    { 
	hiostream_driver_t::reset_attributes();
	print_attribute( '0' ); 
    }
};

class hiostream_t
{
private:
    hiostream_driver_t *driver;

    void print_string( const char *s );
    void print_word( unsigned long val );
    void print_long_long( unsigned long long val );
    void print_double( double val );
    void print_hex( void *val );
    void print_hex( unsigned long val );

protected:
    virtual void print_char( char ch )
	{ if(this->driver) this->driver->print_char(ch); }

    virtual char get_blocking_char()
	{ return (this->driver) ? this->driver->get_blocking_char() : 0; }

public:
    virtual void init( hiostream_driver_t *new_driver )
	{ this->driver = new_driver; }

    hiostream_driver_t *get_driver() { return this->driver; }

    hiostream_driver_t::io_color_e get_color()
    {
	if( this->driver )
	    return this->driver->get_color();
	else
	    return hiostream_driver_t::unknown;
    }

    hiostream_driver_t::io_color_e get_background()
    {
	if( this->driver )
	    return this->driver->get_background();
	else
	    return hiostream_driver_t::unknown;
    }

    hiostream_t& set_color( hiostream_driver_t::io_color_e color )
    { 
	if(this->driver) this->driver->set_color( color ); 
	return *this;
    }

    hiostream_t& set_background( hiostream_driver_t::io_color_e color )
    { 
	if(this->driver) this->driver->set_background( color ); 
	return *this;
    }

    hiostream_t& reset_attributes()
    {
	if(this->driver) this->driver->reset_attributes(); 
	return *this;
    }

    hiostream_t& operator>> (char& ch)
    {
	ch = this->get_blocking_char();
	return *this;
    }

    hiostream_t& operator<< (char *str)
    {
	return *this << (const char *)str;
    }

    hiostream_t& operator<< (const char *str)
    {
	if( str )
	    this->print_string( str );
	else
	    this->print_string( "(null)" );
	return *this;
    }

    hiostream_t& operator<< (void *p);
    hiostream_t& operator<< (long val);
    hiostream_t& operator<< (long long val);

    hiostream_t& operator<< (float val)
    {
	this->print_double(val);
	return *this;
    }

    hiostream_t& operator<< (double val)
    {
	this->print_double(val);
	return *this;
    }

    hiostream_t& operator<< (char ch)
    {
	this->print_char( ch );
	return *this;
    }

    hiostream_t& operator<< (short val)
    {
	return *this << long(val);
    }

    hiostream_t& operator<< (int val)
    {
	return *this << long(val);
    }


    hiostream_t& operator<< (unsigned short val)
    {
	return *this << (unsigned long)val;
    }

    hiostream_t& operator<< (unsigned val)
    {
	return *this << (unsigned long)val;
    }

    hiostream_t& operator<< (unsigned long val)
    {
	print_word( val );
	return *this;
    }
    
    hiostream_t& operator<< (unsigned long long val)
    {
	print_long_long( val );
	return *this;
    }

#if !defined(HCONSOLE_TYPE_SIMPLE)
    hiostream_t& operator<< (L4_ThreadId_t tid);
#endif
};

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

#if defined(cfg_console_serial)
class hiostream_ia32_serial_t : public hiostream_ansi_t
{
public:
    enum comport_e {
	com0 = 0x3f8, com1 = 0x2f8, com2 = 0x3e8, com3 = 0x2e8,
    };

private:
    comport_e comport;

    unsigned char in_u8(const unsigned short port)
    {
	unsigned char tmp;
	__asm__ __volatile__( "inb %1, %0\n" : "=a"(tmp) : "dN"(port) );
	return tmp;
    }

    void out_u8(const unsigned short port, const unsigned char val)
    {
	__asm__ __volatile__( "outb %1, %0\n" : : "dN"(port), "a"(val) );
    }

public:

    virtual void init( comport_e new_comport )
    {
	hiostream_ansi_t::init();
	this->comport = new_comport;
    }

    virtual void print_char( char ch )
    {
	while( !(this->in_u8(this->comport+5) & 0x60) );
	this->out_u8( this->comport, ch );
	if( ch == '\n' )
	    this->print_char( '\r' );
    }

    virtual char get_blocking_char()
    {
	while( (this->in_u8(this->comport+5) & 0x01) == 0 );
	return this->in_u8(this->comport);
    }
};

typedef hiostream_ia32_serial_t hiostream_arch_serial_t;
#endif

#if defined(cfg_console_kdebug)
class hiostream_kdebug_t : public hiostream_driver_t
{
public:
    virtual void print_char( char ch )
	{ L4_KDB_PrintChar( ch ); }

    virtual char get_blocking_char()
	{ return L4_KDB_ReadChar_Blocked(); }
};
#endif

class hiostream_null_t : public hiostream_driver_t
{
public:
    virtual void print_char( char ch ) {}
    virtual char get_blocking_char() { return 0; }
};

extern hconsole_t hout;

#endif	/* __HYPERVISOR__INCLUDE__CONSOLE_H__ */


/*********************************************************************
 *
 * Copyright (C) 2002-2006  Karlsruhe University
 *
 * File path:     common/print.cc
 * Description:   Implementation of printf
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

#include <l4/thread.h>
#include <l4/kip.h>
#include <stdarg.h>	/* for va_list, ... comes with gcc */
#include <common/sync.h>
#include <common/debug.h>
#include <common/ia32/sync.h>

cpu_lock_t console_lock;
char *console_prefix = PREFIX;
static bool newline = true;
static L4_KernelInterfacePage_t * kip = (L4_KernelInterfacePage_t *) 0;
bool l4_tracebuffer_enabled = false;

#define PUTC_BUFLEN 128
char putc_buf[128];
word_t putc_buf_idx = 0;
char assert_string[512] = "ASSERTION:  ";

void set_console_prefix(char *prefix)
{
    console_prefix = prefix;
}

void putc_commit()
{
    putc_buf[putc_buf_idx] = 0;
    L4_KDB_PrintString(putc_buf);
    putc_buf_idx = 0;
}
void putc( const char c )
{
    if (putc_buf_idx == PUTC_BUFLEN-1)
	putc_commit();
    putc_buf[putc_buf_idx++] = c;
}

void assert_hex_to_str( unsigned long val, char *s )
{
    static const char representation[] = "0123456789abcdef";
    bool started = false;
    for( int nibble = 2*sizeof(val) - 1; nibble >= 0; nibble-- )
    {
	unsigned data = (val >> (4*nibble)) & 0xf;
	if( !started && !data )
	    continue;
	started = true;
	*s++ = representation[data] ;
    }
}

void assert_dec_to_str(unsigned long val, char *s)
{
    L4_Word_t divisor;
    int width = 8, digits = 0;

    /* estimate number of spaces and digits */
    for (divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++);

    /* print spaces */
    for ( ; digits < width; digits++ )
	*s++ = ' ';

    /* print digits */
    do {
	*s++ = (((val/divisor) % 10) + '0');
    } while (divisor /= 10);

}


/* convert nibble to lowercase hex char */
#define hexchars(x) (((x) < 10) ? ('0' + (x)) : ('a' + ((x) - 10)))

/**
 *	Print hexadecimal value
 *
 *	@param val		value to print
 *	@param width		width in caracters
 *	@param precision	minimum number of digits to apprear
 *	@param adjleft		left adjust the value
 *	@param nullpad		pad with leading zeros (when right padding)
 *
 *	Prints a hexadecimal value with leading zeroes of given width
 *	using putc(), or if adjleft argument is given, print
 *	hexadecimal value with space padding to the right.
 *
 *	@returns the number of charaters printed (should be same as width).
 */
static int
print_hex(const word_t val,
	  int width = 0,
	  int precision = 0,
	  bool adjleft = false,
	  bool nullpad = false)
{
    int i, n = 0;
    int nwidth = 0;

    // Find width of hexnumber
    while ((val >> (4 * nwidth)) && ((unsigned) nwidth <  2 * sizeof (val)))
	nwidth++;
    if (nwidth == 0)
	nwidth = 1;

    // May need to increase number of printed digits
    if (precision > nwidth)
	nwidth = precision;

    // May need to increase number of printed characters
    if (width == 0 && width < nwidth)
	width = nwidth;

    // Print number with padding
    if (! adjleft)
	for (i = width - nwidth; i > 0; i--, n++)
	    putc(nullpad ? '0' : ' ');
    for (i = 4 * (nwidth - 1); i >= 0; i -= 4, n++)
	putc(hexchars ((val >> i) & 0xF));
    if (adjleft)
	for (i = width - nwidth; i > 0; i--, n++)
	    putc(' ');

    return n;
}

/**
 *	Print a string
 *
 *	@param s	zero-terminated string to print
 *	@param width	minimum width of printed string
 *
 *	Prints the zero-terminated string using putc().  The printed
 *	string will be right padded with space to so that it will be
 *	at least WIDTH characters wide.
 *
 *      @returns the number of charaters printed.
 */
static int
print_string(const char * s, const int width = 0, const int precision = 0)
{
    int n = 0;

    for (;;)
    {
	if (*s == 0)
	    break;

	putc(*s++);
	n++;
	if (precision && n >= precision)
	    break;
    }

    while (n < width) { putc(' '); n++; }

    return n;
}

/**
 *	Print a byte string, ex -- aa:bb:cc:dd:00:11:22
 *
 *	@param s		byte string to print
 *	@param width		minimum width of printed string
 *	@param precision	maximum width of printed string
 *
 *	Prints the byte string using putc().  The printed
 *	string will be right padded with space to so that it will be
 *	at least WIDTH characters wide.
 *
 *      @returns the number of charaters printed.
 */
static int
print_byte_string(const char * s, const int width = 0, const int precision = 0)
{
    int n = 0;

    for( ; n < precision; n++, s++ )
    {
	if (n)
	    putc( ':' );
	putc( hexchars((*s >> 4) & 0xF) );
	putc( hexchars(*s & 0xF) );
    }

    while (n < width) { putc(' '); n++; }

    return n;
}


/**
 *	Print hexadecimal value with a separator
 *
 *	@param val	value to print
 *	@param bits	number of lower-most bits before which to
 *                      place the separator
 *      @param sep      the separator to print
 *
 *	@returns the number of charaters printed.
 */
#if 0
static int
print_hex_sep(const word_t val, const int bits, const char *sep)
{
    int n = 0;

    n = print_hex(val >> bits, 0, 0);
    n += print_string(sep);
    n += print_hex(val & ((1 << bits) - 1), 0, 0);

    return n;
}
#endif


/**
 *	Print decimal value
 *
 *	@param val	value to print
 *	@param width	width of field
 *      @param pad      character used for padding value up to width
 *
 *	Prints a value as a decimal in the given WIDTH with leading
 *	whitespaces.
 *
 *	@returns the number of characters printed (may be more than WIDTH)
 */
static int
print_dec(const unsigned long long val, const int width = 0, const char pad = ' ')
{
    
    L4_Word64_t divisor;
    int digits;

    /* estimate number of spaces and digits */
    for (divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++);

    /* print spaces */
    for ( ; digits < width; digits++ )
	putc(' ');

    /* print digits */
    do {
	putc(((val/divisor) % 10) + '0');
    } while (divisor /= 10);

    /* report number of digits printed */
    return digits;
}


static int
print_double(double val, const int width = 0, const char pad = ' ')
{
    unsigned long word;
    int n = 0;

    if( val < 0.0 )
    {
	putc('-');
	n++;
	val *= -1.0;
    }

    word = (unsigned long)val;
    n += print_dec( word, width, pad);
    val -= (double)word;
    
    if( val <= 0.0 )
	return n;

    putc('.');
    n++;
    
    for( int i = 0; i < 3; i++ )
    {
	val *= 10.0;
	word = (unsigned long)val;
	n += print_dec(word, width, pad);
	val -= (double)word;
    }
    return n;
}

int print_tid (word_t val, word_t width, word_t precision, bool adjleft)
{
    L4_ThreadId_t tid;
    
    tid.raw = val;

    if (tid.raw == 0)
	return print_string ("NIL_THRD", width, precision);

    if (tid.raw == (word_t) -1)
	return print_string ("ANY_THRD", width, precision);

    if (!kip)
	kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface ();

    word_t base_id = 
	tid.global.X.thread_no - kip->ThreadInfo.X.UserBase;
    
    if (base_id < 3)
	{
	    const char *names[3] = { "SIGMA0", "SIGMA1", "ROOTTASK" };
	    return print_string (names[base_id], width, precision);
	}
    // We're dealing with something which is not a special thread ID
    int n = print_hex( tid.raw );
    if( L4_IsGlobalId(tid) ) {
	putc( ' ' ); 
	putc( '<' );
	n += 4 + print_hex( L4_ThreadNo(tid) );
	putc( ':' );
	n += print_hex( L4_Version(tid) );
	putc( '>' );
    }

    return n;
    
}

/**
 *	Does the real printf work
 *
 *	@param format_p		pointer to format string
 *	@param args		list of arguments, variable length
 *
 *	Prints the given arguments as specified by the format string.
 *	Implements a subset of the well-known printf plus some L4-specifics.
 *
 *	@returns the number of characters printed
 */
static int
do_printf(const char* format_p, va_list args)
{
    const char* format = format_p;
    int n = 0;
    int i = 0;
    int l = 0;
    int width = 8;
    int precision = 0;
    bool adjleft = false, nullpad = false;

#define arg(x) va_arg(args, x)

    console_lock.lock();
    
    /* sanity check */
    if (format == 0)
	goto done;

    while (*format)
    {
	if( newline ) {
	    print_string( console_prefix );
	    newline = false;
	}

	switch (*(format))
	{
	case '%':
	    width = precision = 0;
	    adjleft = nullpad = false;
	reentry:
	    switch (*(++format))
	    {
		/* modifiers */
	    case '.':
		for (format++; *format >= '0' && *format <= '9'; format++)
		    precision = precision * 10 + (*format) - '0';
		if (*format == 'w')
		{
		    // Set precision to printsize of a hex word
		    precision = sizeof (word_t) * 2;
		    format++;
		}
		format--;
		goto reentry;
	    case '0':
		nullpad = (width == 0);
	    case '1'...'9':
		width = width*10 + (*format)-'0';
		goto reentry;
	    case 'w':
		// Set width to printsize of a hex word
		width = sizeof (word_t) * 2;
		goto reentry;
	    case '-':
		adjleft = true;
		goto reentry;
	    case 'l':
		l++;
		goto reentry;
		break;
	    case 'c':
		putc(arg(int));
		n++;
		break;
	    case 'd':
	    {
		long long val = (l >= 2) ? arg(long long):arg(long);
		if (val < 0)
		{
		    putc('-');
		    val = -val;
		}
		n += print_dec(val, width);
		break;
	    }
	    case 'u':
		n += print_dec((l >= 2) ? arg(long long):arg(long), width);
		break;
	    case 'p':
		precision = sizeof (word_t) * 2;
	    case 'x':
		n += print_hex((l >= 2) ? arg(long long):arg(long), 
			width, precision, adjleft, nullpad);
		break;
	    case 'f':
		n += print_double(arg(double), width, nullpad);
		break;
	    case 'b':
	    {
		char* s = arg(char*);
		if (s)
		    n += print_byte_string(s, width, precision);
		else
		    n += print_string("(null)", width, precision);
	    }
	    break;
	    case 's':
	    {
		char* s = arg(char*);
		if (s)
		    n += print_string(s, width, precision);
		else
		    n += print_string("(null)", width, precision);
	    }
	    break;
	    case 't':
	    case 'T':
		n += print_tid (arg (word_t), width, precision, adjleft);
		break;

	    case '%':
		putc('%');
		n++;
		format++;
		continue;
	    default:
		n += print_string("?");
		break;
	    };
	    i++;
	    break;

	case '\n':
	    newline = true;
	    // fall through
	default:
	    putc(*format);
	    n++;
	    break;
	}
	format++;
    }

done:
    console_lock.unlock();
    putc_commit();
    return n;
}

/**
 *	Flexible print function
 *
 *	@param format	string containing formatting and parameter type
 *			information
 *	@param ...	variable list of parameters
 *
 *	@returns the number of characters printed
 */
extern "C" int
l4kdb_printf(const char* format, ...)
{
    va_list args;
    int i;

    va_start(args, format);
    {
      i = do_printf(format, args);
    }
    va_end(args);
    return i;
};


/**
 *	Flexible print function
 *
 *	@param format	string containing formatting and parameter type
 *			information
 *	@param ...	variable list of parameters
 *
 *	@returns the number of characters printed
 */


extern "C" int 
trace_printf(word_t debug_level, const char* format, ...)	       
{
#if defined(L4_TRACEBUFFER)
    va_list args;
    word_t arg;
    int i;

    word_t id = (word_t) format & 0xffff;
    id += L4_TRACEBUFFER_USERID_START;

    
    word_t type = max((word_t) debug_level, (word_t) DBG_LEVEL) - DBG_LEVEL;
    type = 1 << type;
    
    word_t addr = __L4_TBUF_GET_NEXT_RECORD (type, id);

    if (addr == 0)
	return 0;

    va_start(args, format);
    
    __L4_TBUF_STORE_STR (addr, format);
    
    for (i=0; i < L4_TRACEBUFFER_NUM_ARGS; i++)
    {
	arg = va_arg(args, word_t);
	if (arg == L4_TRACEBUFFER_MAGIC)
	    break;
	
	__L4_TBUF_STORE_DATA(addr, i, arg);
    }
    va_end(args);
    return 0;
#endif
}


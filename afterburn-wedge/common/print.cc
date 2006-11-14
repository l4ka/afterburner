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

#include INC_ARCH(types.h)

#include <stdarg.h>	/* for va_list, ... comes with gcc */
#include <console.h>

console_putc_t console_putc = NULL;
const char *console_prefix = NULL;

static bool newline = true;

#if defined(CONFIG_WEDGE_L4KA)
#include INC_WEDGE(sync.h)
cpu_lock_t console_lock;
#endif


INLINE void lock_console()
{
#if defined(CONFIG_WEDGE_L4KA)
    console_lock.lock();
#endif
}

INLINE void unlock_console()
{
#if defined(CONFIG_WEDGE_L4KA)
    console_lock.unlock();
#endif
}


void console_init( console_putc_t putc, const char *prefix )
{
    console_putc = putc;
    console_prefix = prefix;
#if defined(CONFIG_WEDGE_L4KA)
    console_lock.init();
#endif
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
	    console_putc(nullpad ? '0' : ' ');
    for (i = 4 * (nwidth - 1); i >= 0; i -= 4, n++)
	console_putc(hexchars ((val >> i) & 0xF));
    if (adjleft)
	for (i = width - nwidth; i > 0; i--, n++)
	    console_putc(' ');

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

	console_putc(*s++);
	n++;
	if (precision && n >= precision)
	    break;
    }

    while (n < width) { console_putc(' '); n++; }

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
	    console_putc( ':' );
	console_putc( hexchars((*s >> 4) & 0xF) );
	console_putc( hexchars(*s & 0xF) );
    }

    while (n < width) { console_putc(' '); n++; }

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
print_dec(const word_t val, const int width = 0, const char pad = ' ')
{
    word_t divisor;
    int digits;

    /* estimate number of spaces and digits */
    for (divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++);

    /* print spaces */
    for ( ; digits < width; digits++ )
	console_putc(' ');

    /* print digits */
    do {
	console_putc(((val/divisor) % 10) + '0');
    } while (divisor /= 10);

    /* report number of digits printed */
    return digits;
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
    int width = 8;
    int precision = 0;
    bool adjleft = false, nullpad = false;

#define arg(x) va_arg(args, x)

    /* sanity check */
    if (format == 0)
	return 0;

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
		goto reentry;
		break;
	    case 'c':
		console_putc(arg(int));
		n++;
		break;
	    case 'd':
	    {
		long val = arg(long);
		if (val < 0)
		{
		    console_putc('-');
		    val = -val;
		}
		n += print_dec(val, width);
		break;
	    }
	    case 'u':
		n += print_dec(arg(long), width);
		break;
	    case 'p':
		precision = sizeof (word_t) * 2;
	    case 'x':
		n += print_hex(arg(long), width, precision,
			       adjleft, nullpad);
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

#if 0
	    case 't':
	    case 'T':
		n += print_tid (arg (word_t), width, precision, adjleft);
		break;
#endif

	    case '%':
		console_putc('%');
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
	    console_putc(*format);
	    n++;
	    break;
	}
	format++;
    }

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
printf(const char* format, ...)
{
    va_list args;
    int i;

    va_start(args, format);
    lock_console();
    {
      i = do_printf(format, args);
    }
    unlock_console();
    va_end(args);
    return i;
};


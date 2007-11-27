/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/startup.cc
 * Description:   C runtime initialization.  This has *the* entry point
 *                invoked by the boot loader.
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

// This file is compiled in such a way that all code/symbols/etc defined here
// live at very low addresses, compared to the rest of the wedge. This is due
// to some restrictions in xen and/or the domain builder I don't really
// understand. If you know more about it, please contact me!
// Specifically, this means that we cannot access symbols defined in other
// files easily, and thus we cannot even use the normal hout printing! Inline
// functions etc work, though.

#include INC_ARCH(types.h)
#include INC_ARCH(cpu.h)
#include INC_WEDGE(xen_hypervisor.h)

extern "C" void afterburn_c_runtime_init_high( start_info_t *xen_info, word_t boot_stack );

#define DEBUG_THIS 0

// The following code is copied mostly verbatim from common/startup.cc and
// needed only for debugging purpose.
// Although printf ought to be avoided, it doesn't seem worthwile to
// implement iostream-like output here.
#include <stdarg.h>	/* for va_list, ... comes with gcc */
static void
console_putc (char c)
{
#if DEBUG_THIS
    XEN_console_io( CONSOLEIO_write, 1, &c );
#endif
}

static bool newline = true;
static const char *console_prefix = "\e[1m\e[37m"
                                    CONFIG_CONSOLE_PREFIX
				    " early:\e[0m ";

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


// XXX this is broken as it assumes sizeof(int) == sizeof(long), try e.g. %d
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
static int
printf(const char* format, ...)
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

static void
dump_startinfo( start_info_t *xen_info )
{
    printf( "start info:\n" );
    printf( "  magic:        %s\n", xen_info->magic );
    printf( "  nr_pages:     %lu\n", xen_info->nr_pages );
    printf( "  shared_info:  %p\n", xen_info->shared_info );
    printf( "  flags:        %x\n", xen_info->flags );
    printf( "  pt_base:      %p\n", xen_info->pt_base );
    printf( "  nr_pt_frames: %lu\n", xen_info->nr_pt_frames );
    printf( "  mfn_list:     %p\n", xen_info->mfn_list );
    printf( "  cmd_line:     %s\n", xen_info->cmd_line );
}

static void
print_indent( unsigned ind )
{
    for( unsigned i = 0;i < ind;++i )
	printf( " " );
}

unsigned long nr_pages;
word_t* mfn_list;

static word_t
phys_to_machine( void *addr )
{
    return mfn_list[((word_t)addr) >> PAGE_BITS] << PAGE_BITS;
}

static void*
machine_to_phys( word_t addr )
{
    void *r = (void *)(machine_to_phys_mapping[addr>>PAGE_BITS]<<PAGE_BITS);
    // sanity check, this is necessary!
    if( ((word_t)r) >> PAGE_BITS > nr_pages || phys_to_machine( r ) != addr)
	return 0;
    return r;
}

static void
dump_pgent( pgent_t *pt, unsigned level, unsigned indent, unsigned thr )
{
    if( level < thr )
	return;

    if( !pt->is_valid() )
	return;

    print_indent( indent );
    printf( "%3u:", (((word_t)pt) & 4095) / 8 );

#define TF(a, b) \
    if( pt->is_##a() ) \
    	printf( " %s", b );

    TF( valid, "p" );
    TF( writable, "rw" );
    TF( superpage, "2m" );

#undef TF

    printf( " -> %p\n", pt->get_address() );
    if( level == 1 ) // leaf entry
	return;

    pgent_t* next = (pgent_t *)machine_to_phys( pt->get_address() );
    if (!next)
	return; // not accessible

    for( unsigned i = 0;i < 512;++i)
	dump_pgent( next + i, level - 1, indent + 8, thr );
}

static void
dump_pt( pgent_t *pt )
{
    printf( "-------------begin page table dump------------\n" );
    for( unsigned i = 0;i < 512;++i)
	dump_pgent( pt + i, 4, 0, 2 );
    printf( "-------------page table dump finished---------\n" );
}

// Insert an entry in slot 511 of PT equal to slop 0 of PT
static void
adjust_mapping( pgent_t* pt )
{
    mmu_update_t mmu_updates[1];
    mmu_updates[0].ptr = phys_to_machine( pt ) + 511 * sizeof( pgent_t );
    mmu_updates[0].val = pt[0].get_raw();
    int r = XEN_mmu_update( mmu_updates, 1, 0 );
    if( r < 0 ) {
	printf( "XEN_mmu_update failed: %d\n", r );
	// TODO PANIC
    }
}

extern "C" NORETURN void 
afterburn_c_runtime_init( start_info_t *xen_info, word_t boot_stack )
{
    // On startup, xen allocates our memory and inserts the pages into the
    // mfn_list to form our pseudo-physical memory. Then, a multiple of 2M
    // of the pseudo-physical memory is mapped 1:1 into our virtual address
    // space to contain our boot data structures (initial page table etc).
    // The initial page table must look something like this:
    //
    // pml4
    // 0   --> pdp
    //         0 --> pdir
    //               0 --> ptab
    //               1 --> ptab
    //
    // We manipulate it to be able to access our boot code at very high
    // addresses, where we are mapped by default:
    //
    // pml4
    // 0   --> pdp
    //     |   0   --> pdir
    //     |       |   0 --> ptab <-----
    //     |       |   1 --> ptab      | 
    //     |       |                   |
    //     |       |   511 ------------/
    //     |   511 -
    // 511 -
    //
    // In effect, we alias the first 1M of virtual memory a number of times.

    printf( "hello: %p %p\n", xen_info, boot_stack );
    dump_startinfo( xen_info );

    mfn_list = (word_t *)xen_info->mfn_list;
    nr_pages = xen_info->nr_pages;
    // *_to_* work now

    pgent_t* pt = (pgent_t *)xen_info->pt_base;
    dump_pt( pt );

    // adjust mappings
    // fixup pml4
    adjust_mapping( pt );
    //dump_pt( (pgent_t *)xen_info->pt_base );
    // fixup pdp
    pt = (pgent_t *)machine_to_phys( pt->get_address() );
    adjust_mapping( pt );
    //dump_pt( (pgent_t *)xen_info->pt_base );
    // fixup pdir
    pt = (pgent_t *)machine_to_phys( pt->get_address() );
    adjust_mapping( pt );
    dump_pt( (pgent_t *)xen_info->pt_base );

    // XXX er, why does that work?
    afterburn_c_runtime_init_high( xen_info, boot_stack );

    while( 1 )
	XEN_yield();
}

char xen_hypervisor_config_string[] SECTION("__xen_guest") =
    "GUEST_OS=kaxen,LOADER=generic,GUEST_VER=0.1"
#if defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    ",PT_MODE_WRITABLE"
#endif
#if defined(CONFIG_XEN_2_0)
    ",XEN_VER=2.0"
#elif defined(CONFIG_XEN_3_0)
    ",XEN_VER=xen-3.0"
#else
# error better fix that bootloader insanity
#endif
    ;

// (Put the stack in a special section so that clearing bss doesn't clear
// the stack.) This doesn't apply any longer, as the bss is cleared after
// the stack is switched, but..
__asm__ (
    ".section .afterburn.stack,\"aw\"\n"
    ".balign 16\n"
    "kaxen_wedge_low_stack:\n"
    ".space 32*1024\n"
    "kaxen_wedge_low_stack_top:\n"
);

__asm__ (
    ".text\n"
    ".globl kaxen_wedge_start\n"
    "kaxen_wedge_start:\n"
    "	cld\n"
    "	movq	%rsp, %rax\n" // Remember the stack provided by Xen.
    "	leaq	kaxen_wedge_low_stack_top, %rsp\n"
    "	movq	%rsi, %rdi\n" // The startup info (arg1).
    "	movq	%rax, %rsi\n" // arg2 (xen stack)
    "	call	afterburn_c_runtime_init\n"
);


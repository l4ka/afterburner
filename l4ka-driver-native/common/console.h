#ifndef __COMMON__CONSOLE_H__
#define __COMMON__CONSOLE_H__

typedef void (*console_putc_t)(const char c);

extern void kdebug_putc( const char c );

extern void console_init( console_putc_t putc, const char *prefix=NULL );

extern "C" int printf(const char *format, ...);

INLINE const char *get_console_prefix()
{
    extern const char *console_prefix;
    return console_prefix;
}

#endif	/* __COMMON__CONSOLE_H__ */

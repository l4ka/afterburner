
#include <common/console.h>
#include <l4/kdebug.h>

console_putc_t console_putc;
const char *console_prefix = NULL;

void kdebug_putc( const char c )
{
    L4_KDB_PrintChar( c );
}

void console_init( console_putc_t putc, const char *prefix )
{
    console_putc = putc;
    console_prefix = prefix;
}


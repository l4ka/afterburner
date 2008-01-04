#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONSOLE_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONSOLE_H__

#include INC_WEDGE(iostream.h)
extern "C" int dbg_printf(const char* format, ...);		

#define dprintf(n,a...)						\
    do								\
    {								\
	if(DBG_LEVEL>n)						\
	    dbg_printf(a);					\
    } while(0)



#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONSOLE_H__ */

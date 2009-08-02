#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONSOLE_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONSOLE_H__

#include INC_WEDGE(iostream.h)

class debug_id_t
{
public:
    word_t id;
    word_t level;

    debug_id_t (word_t i, word_t l) { id = i; level = l; }

    inline debug_id_t operator + (const int &n) const
	{
	    return debug_id_t(id, level+n);
	}

    inline operator bool (void) const
	{ 
	    return level;
	}


};


extern "C" int dbg_printf(const char* format, ...);		

#define dprintf(id,a...)					\
    do								\
    {								\
	if((id).level<DBG_LEVEL)				\
	    dbg_printf(a);					\
    } while(0)



#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__CONSOLE_H__ */

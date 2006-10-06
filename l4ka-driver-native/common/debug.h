#ifndef __COMMON__DEBUG_H__
#define __COMMON__DEBUG_H__

extern "C" NORETURN void panic( void );

#if defined(cfg_optimize)
#define ASSERT(x)
#else
#define ASSERT(x)						\
    do {							\
	if(EXPECT_FALSE(!(x))) {				\
	    printf( "Assertion: %s,\nfile %s : %u,\nfunc %s\n",	\
		    MKSTR(x), __FILE__, __LINE__, __func__ );	\
	    panic();						\
	}							\
    } while(0)
#endif

#define PANIC(str)						\
    do {							\
	printf( "Panic: %s,\nfile %s : %u,\nfunc %s\n",		\
		str, __FILE__, __LINE__, __func__ );		\
	panic();						\
    } while(0);

#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");

#endif	/* __COMMON__DEBUG_H__ */

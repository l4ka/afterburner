#ifndef __AFTERBURN_SYSCALLS_H__
#define __AFTERBURN_SYSCALLS_H__

#include "types.h"

typedef enum {
    afterburn_no_err=0, afterburn_err_addr=1, 
    afterburn_err_illegal_syscall=2,
} afterburn_err_e;


static inline word_t afterburn_get_wedge_counters( 
	word_t start, word_t *counters, word_t cnt,
	afterburn_err_e *error )
{
    word_t result, dummy1, dummy2;
    *error = afterburn_no_err;
    __asm__ __volatile__ (
	    "int	$0x69		\n"
	    "1: mov	%%eax, %3	\n"
	    ". = 1b + 6			\n"
	    "2:				\n"
	    : "=a"(result), "=d"(dummy1), "=c"(dummy2), "=m"(*error)
	    : "0"(0), "1"(start), "2"(counters), "b"(cnt)
	    : "memory"
	    );
    return result;
}

static inline word_t afterburn_get_kernel_counters( 
	word_t start, word_t *counters, word_t cnt, 
	afterburn_err_e *error )
{
    word_t result, dummy1, dummy2;
    *error = afterburn_no_err;
    __asm__ __volatile__ (
	    "int	$0x69		\n"
	    "1: mov	%%eax, %3	\n"
	    ". = 1b + 6			\n"
	    "2:				\n"
	    : "=a"(result), "=d"(dummy1), "=c"(dummy2), "=m"(*error)
	    : "0"(1), "1"(start), "2"(counters), "b"(cnt)
	    : "memory"
	    );
    return result;
}

static inline void afterburn_zero_wedge_counters( afterburn_err_e *error )
{
    word_t dummy1, dummy2, dummy3;
    *error = afterburn_no_err;
    __asm__ __volatile__ (
	    "int	$0x69		\n"
	    "1: mov	%%eax, %3	\n"
	    ". = 1b + 6			\n"
	    "2:				\n"
	    : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "=m"(*error)
	    : "0"(2)
	    : "memory"
	    );
}

static inline void afterburn_zero_kernel_counters( afterburn_err_e *error )
{
    word_t dummy1, dummy2, dummy3;
    *error = afterburn_no_err;
    __asm__ __volatile__ (
	    "int	$0x69		\n"
	    "1: mov	%%eax, %3	\n"
	    ". = 1b + 6			\n"
	    "2:				\n"
	    : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "=m"(*error)
	    : "0"(3)
	    : "memory"
	    );
}

#endif	/* __AFTERBURN_SYSCALLS_H__ */

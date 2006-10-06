
#ifndef __COMMON__MACROS_H__
#define __COMMON__MACROS_H__

#if !defined(ASSEMBLY)

#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 3)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define INLINE extern inline

#if (__GNUC__ >= 3)
#define EXPECT_FALSE(x)		__builtin_expect((x), false)
#define EXPECT_TRUE(x)		__builtin_expect((x), true)
#define EXPECT_VALUE(x,val)	__builtin_expect((x), (val))
#else
#define EXPECT_FALSE(x)		(x)
#define EXPECT_TRUE(x)		(x)
#define EXPECT_VALUE(x,val)	(x)
#endif

#define SECTION(x) __attribute__((section(x)))
#define NORETURN __attribute__((noreturn))
#define ALIGNED(x) __attribute__((aligned(x)))
#define WEAK __attribute__(( weak ))
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1)
#define NOINLINE __attribute__ ((noinline))
#else
#define NOINLINE
#endif

/* Convenience functions for memory sizes. */
#define KB(x)	((typeof(x)) (word_t(x) * 1024))
#define MB(x)	((typeof(x)) (word_t(x) * 1024*1024))
#define GB(x)	((typeof(x)) (word_t(x) * 1024*1024*1024))

#define offsetof(type, field) (word_t(&((type *)0x1000)->field) - 0x1000)

#endif


/* Turn preprocessor symbol definition into string */
#define MKSTR(sym)	MKSTR2(sym)
#define MKSTR2(sym)	#sym

/* Safely "append" an UL suffix for also asm values */
#if defined(ASSEMBLY)
#define UL(x)		x
#else
#define UL(x)		x##UL
#endif

#ifndef NULL
#define NULL	0
#endif

#endif	/* __COMMON__MACROS_H__ */

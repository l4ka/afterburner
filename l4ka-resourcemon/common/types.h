#ifndef __L4KA_RESOURCEMON__COMMON__TYPES_H__
#define __L4KA_RESOURCEMON__COMMON__TYPES_H__

#include <l4/types.h>

typedef unsigned int __attribute__((__mode__(__DI__))) u64_t;
typedef unsigned int __attribute__((__mode__(__SI__))) u32_t;
typedef unsigned int __attribute__((__mode__(__HI__))) u16_t;
typedef unsigned int __attribute__((__mode__(__QI__))) u8_t;

typedef signed int __attribute__((__mode__(__DI__))) s64_t;
typedef signed int __attribute__((__mode__(__SI__))) s32_t;
typedef signed int __attribute__((__mode__(__HI__))) s16_t;
typedef signed int __attribute__((__mode__(__QI__))) s8_t;

typedef unsigned long word_t;

#endif	/* __L4KA_RESOURCEMON__COMMON__TYPES_H__ */

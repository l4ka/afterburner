#ifndef __COMMON__IA32__PAGE_H__
#define __COMMON__IA32__PAGE_H__

#define PAGE_BITS	12
#define PAGE_SIZE	(UL(1) << PAGE_BITS)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#endif	/* __COMMON__IA32__PAGE_H__ */

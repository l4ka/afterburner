#ifndef __COMMON__IA32__PORTS_H__
#define __COMMON__IA32__PORTS_H__

/**
 * out_u8:	outputs a byte on an io port
 * @port:	port number
 * @val:	value
 */
extern inline void out_u8(const u16_t port, const u8_t val)
{
    /* GCC can optimize here if constant */
    if (__builtin_constant_p(port) && (port < 0x100))
	__asm__ __volatile__("outb	%1, %0\n"
			     :
			     : "dN"(port), "a"(val));
    else
	__asm__ __volatile__("outb	%1, %%dx\n"
			     :
			     : "d"(port), "a"(val));
}

/**
 * in_u8:	reads a byte from a port
 * @port:	port number
 *
 * Returns the read byte.
 */
extern inline u8_t in_u8(const u16_t port)
{
    u8_t tmp;
    /* GCC can optimize here if constant */
    if (__builtin_constant_p(port) && (port < 0x100))
	__asm__ __volatile__("inb %1, %0\n"
			     : "=a"(tmp)
			     : "dN"(port));
    else
	__asm__ __volatile__("inb %%dx, %0\n"
			     : "=a"(tmp)
			     : "d"(port));
    return tmp;
}

/**
 * out_u16:	outputs a 16bit value on an io port
 * @port:	port number
 * @val:	value
 */
extern inline void out_u16(const u16_t port, const u16_t val)
{
    /* GCC can optimize here if constant */
    if (__builtin_constant_p(port) && (port < 0x100))
	__asm__ __volatile__("outw	%1, %0\n"
			     :
			     : "dN"(port), "a"(val));
    else
	__asm__ __volatile__("outw	%1, %%dx\n"
			     :
			     : "d"(port), "a"(val));
}

/**
 * in_u8:	reads a 16bit value from a port
 * @port:	port number
 *
 * Returns the read 16bit value.
 */
extern inline u16_t in_u16(const u16_t port)
{
    u16_t tmp;
    /* GCC can optimize here if constant */
    if (__builtin_constant_p(port) && (port < 0x100))
	__asm__ __volatile__("inw %1, %0\n"
			     : "=a"(tmp)
			     : "dN"(port));
    else
	__asm__ __volatile__("inw %%dx, %0\n"
			     : "=a"(tmp)
			     : "d"(port));
    return tmp;
};

/**
 * out_u32:	outputs a 32bit value on an io port
 * @port:	port number
 * @val:	value
 */
extern inline void out_u32(const u16_t port, const u32_t val)
{
    /* GCC can optimize here if constant */
    if (__builtin_constant_p(port) && (port < 0x100))
	__asm__ __volatile__("outl	%1, %0\n"
			     :
			     : "dN"(port), "a"(val));
    else
	__asm__ __volatile__("outl	%1, %%dx\n"
			     :
			     : "d"(port), "a"(val));
}

/**
 * in_u8:	reads a 32bit value from a port
 * @port:	port number
 *
 * Returns the read 32bit value.
 */
extern inline u32_t in_u32(const u16_t port)
{
    u32_t tmp;
    /* GCC can optimize here if constant */
    if (__builtin_constant_p(port) && (port < 0x100))
	__asm__ __volatile__("inl %1, %0\n"
			     : "=a"(tmp)
			     : "dN"(port));
    else
	__asm__ __volatile__("inl %%dx, %0\n"
			     : "=a"(tmp)
			     : "d"(port));
    return tmp;
};


#endif /* __COMMON__IA32__PORTS_H__ */

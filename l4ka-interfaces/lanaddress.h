/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:	interfaces/lanaddress.h
 * Description:   
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/
#ifndef __INTERFACES__LANADDRESS_H__
#define __INTERFACES__LANADDRESS_H__

#include <l4/types.h>

#define LANADDRESS_LEN	6

typedef union
{
    struct {
    	// For 2-byte aligned addresses.
	L4_Word16_t lsb;
	L4_Word32_t msb __attribute__ ((packed));
    } align2;

    struct {
	// For 4-byte aligned addresses.
	L4_Word32_t lsb __attribute__ ((packed));
	L4_Word16_t msb;
    } align4;

    struct {
	L4_Word8_t group : 1;  // multicast if set
	L4_Word8_t local : 1;  // local if set
	L4_Word8_t : 6;
    } status;

    L4_Word16_t halfwords[LANADDRESS_LEN/sizeof(L4_Word16_t)];
    L4_Word8_t raw[LANADDRESS_LEN];

} lanaddress_t;


extern inline void
lanaddress_set_handle( lanaddress_t *lanaddr, L4_Word16_t handle )
{
    lanaddr->halfwords[LANADDRESS_LEN/sizeof(L4_Word16_t) - 1] = handle;
}

extern inline L4_Word16_t
lanaddress_get_handle( lanaddress_t *lanaddr )
{
    return lanaddr->halfwords[LANADDRESS_LEN/sizeof(L4_Word16_t) - 1];
}

extern inline int
lanaddress_is_dst_broadcast( lanaddress_t *lanaddr )
{
    return (lanaddr->align2.lsb == 0xffff) &&
	   (lanaddr->align2.msb == 0xffffffff);
}

#endif	/* __INTERFACES__LANADDRESS_H__ */

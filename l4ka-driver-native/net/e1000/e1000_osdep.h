/*******************************************************************************


  Copyright(c) 1999 - 2002 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


/* glue for the OS independant part of e1000
 * includes register access macros
 */

#ifndef _E1000_OSDEP_H_
#define _E1000_OSDEP_H_

#include <l4/types.h>
#include <l4/ipc.h>

typedef u64_t 	uint64_t;
typedef u32_t	uint32_t;
typedef u16_t	uint16_t;
typedef u8_t	uint8_t;

typedef s64_t 	int64_t;
typedef s32_t 	int32_t;
typedef s16_t	int16_t;
typedef s8_t	int8_t;

#if defined(DEBUG)
#define DBG 1
#define BUG()	do { printf( "e1000 assert %s : %s : %d\n", __FILE__, __FUNCTION__, __LINE__); L4_KDB_Enter("e1000 assert"); } while(0)
#else
#define BUG()
#endif

#define usec_delay(x)	do { int i; for(i=0; i<((x)*1000); i++) ; } while(0)
#define msec_delay(x)	L4_Sleep( L4_TimePeriod(((L4_Word64_t)x)*1000) )

#define PCI_COMMAND_REGISTER   0x04
#define CMD_MEM_WRT_INVALIDATE 0x10

typedef enum {
    FALSE = 0,
    TRUE = 1
} boolean_t;

#define MSGOUT(S, A, B)	printf(S "\n", A, B)

#if DBG
#define DEBUGOUT(S)		printf(S "\n")
#define DEBUGOUT1(S, A...)	printf(S "\n", A)
#else
#define DEBUGOUT(S)
#define DEBUGOUT1(S, A...)
#endif

#define DEBUGFUNC(F) DEBUGOUT(F)
#define DEBUGOUT2 DEBUGOUT1
#define DEBUGOUT3 DEBUGOUT2
#define DEBUGOUT7 DEBUGOUT3

/* Mimic Linux io access stuff for the x86. */
#define __io_virt(x) ((void *)(x))
#define writel(b,addr) (*(volatile unsigned int *) __io_virt(addr) = (b))
#define readl(addr) (*(volatile unsigned int *) __io_virt(addr))



#define E1000_WRITE_REG(a, reg, value) ( \
    ((a)->mac_type >= e1000_82543) ? \
	(writel((value), ((a)->hw_addr + E1000_##reg))) : \
	(writel((value), ((a)->hw_addr + E1000_82542_##reg))))

#define E1000_READ_REG(a, reg) ( \
    ((a)->mac_type >= e1000_82543) ? \
	readl((a)->hw_addr + E1000_##reg) : \
	readl((a)->hw_addr + E1000_82542_##reg))

#define E1000_WRITE_REG_ARRAY(a, reg, offset, value) ( \
    ((a)->mac_type >= e1000_82543) ? \
	writel((value), ((a)->hw_addr + E1000_##reg + ((offset) << 2))) : \
	writel((value), ((a)->hw_addr + E1000_82542_##reg + ((offset) << 2))))

#define E1000_READ_REG_ARRAY(a, reg, offset) ( \
    ((a)->mac_type >= e1000_82543) ? \
	readl((a)->hw_addr + E1000_##reg + ((offset) << 2)) : \
	readl((a)->hw_addr + E1000_82542_##reg + ((offset) << 2)))

#define E1000_WRITE_FLUSH(a) E1000_READ_REG(a, STATUS)

#endif /* _E1000_OSDEP_H_ */

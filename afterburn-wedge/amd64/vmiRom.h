/*
 * afterburn-wedge/amd64/vmiRom.h
 *
 *      Header file for paravirtualization interface and definitions
 *      for the hypervisor option ROM tables.
 *
 *      Copyright (C) 2005, VMWare, Inc.
 *      Copyright (C) 2006, University of Karlsruhe
 *
 */

#ifndef __AMD64__VMIROM_H__
#define __AMD64__VMIROM_H__

// XXX unmodified copy of ia32 version

#include "vmiCalls.h"

/*
 *---------------------------------------------------------------------
 *
 *  VMI Option ROM API
 *
 *---------------------------------------------------------------------
 */
#define VDEF(call) VMI_CALL_##call,
typedef enum VMICall {
   VMI_CALLS
   NUM_VMI_CALLS
} VMICall;
#undef VDEF

#define VMI_SIGNATURE 0x696d5663   /* "cVmi" */

#define VMI_API_REV_MAJOR          13
#define VMI_API_REV_MINOR          0

/* Flags used by VMI_Reboot call */
#define VMI_REBOOT_SOFT          0x0
#define VMI_REBOOT_HARD          0x1

/* Flags used by VMI_{Notify|Release}Page call */
#define VMI_PAGE_PT              0x01
#define VMI_PAGE_PD              0x02
#define VMI_PAGE_PAE             0x04
#define VMI_PAGE_PDP             0x04
#define VMI_PAGE_PML4            0x08

/* Flags used by VMI_FlushTLB call */
#define VMI_FLUSH_TLB            0x01
#define VMI_FLUSH_GLOBAL         0x02

/* Flags used by VMI_FlushSync call */
#define VMI_FLUSH_PT_UPDATES     0x80

/* The number of VMI address translation slot */
#define VMI_LINEAR_MAP_SLOTS    4

/* The cycle counters. */
#define VMI_CYCLES_REAL         0
#define VMI_CYCLES_AVAILABLE    1
#define VMI_CYCLES_STOLEN       2

/* The alarm interface 'flags' bits. [TBD: exact format of 'flags'] */
#define VMI_ALARM_COUNTERS      2

#define VMI_ALARM_COUNTER_MASK  0x000000ff

#define VMI_ALARM_WIRED_IRQ0    0x00000000
#define VMI_ALARM_WIRED_LVTT    0x00010000

#define VMI_ALARM_IS_ONESHOT    0x00000000
#define VMI_ALARM_IS_PERIODIC   0x00000100


/*
 *---------------------------------------------------------------------
 *
 *  Generic VROM structures and definitions
 *
 *---------------------------------------------------------------------
 */

/* VROM call table definitions */
#define VROM_CALL_LEN             32

typedef struct VROMCallEntry {
   char f[VROM_CALL_LEN];
} VROMCallEntry;

typedef struct VROMHeader {
   VMI_UINT16          romSignature;             // option ROM signature
   VMI_INT8            romLength;                // ROM length in 512 byte chunks
   unsigned char       romEntry[4];              // 16-bit code entry point
   VMI_UINT8           romPad0;                  // 4-byte align pad
   VMI_UINT32          vRomSignature;            // VROM identification signature
   VMI_UINT8           APIVersionMinor;          // Minor version of API
   VMI_UINT8           APIVersionMajor;          // Major version of API
   VMI_UINT8           reserved0;                // Reserved for expansion
   VMI_UINT8           reserved1;                // Reserved for expansion
   VMI_UINT32          reserved2;                // Reserved for expansion
   VMI_UINT32          reserved3;                // Reserved for private use
   VMI_UINT16          pciHeaderOffset;          // Offset to PCI OPROM header
   VMI_UINT16          pnpHeaderOffset;          // Offset to PnP OPROM header
   VMI_UINT32          romPad3;                  // PnP reserverd / VMI reserved
   VROMCallEntry       romCallReserved[3];       // Reserved call slots
} VROMHeader;

typedef struct VROMCallTable {
   VROMCallEntry    vromCall[128];           // @ 0x80: ROM calls 4-127
} VROMCallTable;

/* State needed to start an application processor in an SMP system. */
typedef struct APState {
   VMI_UINT32 cr0;
   VMI_UINT32 cr2;
   VMI_UINT32 cr3;
   VMI_UINT32 cr4;

   VMI_UINT64 efer;

   VMI_UINT32 eip;
   VMI_UINT32 eflags;
   VMI_UINT32 eax;
   VMI_UINT32 ebx;
   VMI_UINT32 ecx;
   VMI_UINT32 edx;
   VMI_UINT32 esp;
   VMI_UINT32 ebp;
   VMI_UINT32 esi;
   VMI_UINT32 edi;
   VMI_UINT16 cs;
   VMI_UINT16 ss;
   VMI_UINT16 ds;
   VMI_UINT16 es;
   VMI_UINT16 fs;
   VMI_UINT16 gs;
   VMI_UINT16 ldtr;

   VMI_UINT16 gdtr_limit;
   VMI_UINT32 gdtr_base;
   VMI_UINT32 idtr_base;
   VMI_UINT16 idtr_limit;
} APState;

#endif	/* __AMD64__VMIROM_H__ */

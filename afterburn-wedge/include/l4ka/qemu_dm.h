/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/device/ide.h
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

#ifndef __QEMU_DM_H__
#define __QEMU_DM_H__

#include INC_ARCH(types.h)
#include <debug.h>
#include INC_WEDGE(l4thread.h)
#include <console.h>
#include <string.h>

#include <qemu-dm_idl_client.h>
#include <qemu-dm_pager_idl_server.h>

typedef u64_t uint64_t;
typedef u32_t uint32_t;
typedef u16_t uint16_t;
typedef u8_t  uint8_t;
typedef s64_t int64_t;
typedef s32_t int32_t;
typedef s16_t int16_t;
typedef s8_t int8_t;

#define PASS_THROUGH_PORTS 	/* Programmable interrupt controller */	\
    case 0x20 ... 0x21:						\
    case 0xa0 ... 0xa1:				\
    case 0x4d0 ... 0x4d1:			\
    case 0x70 ... 0x71: /* RTC */		\
    case 0xe9:		/* VGA BIOS debug ports */ \
    case 0x1ce ... 0x1cf:   /* VGA */				\
    case 0x3b0 ... 0x3df:    /* VGA */				\
    case 0x400 ... 0x403: /* BIOS debug ports */			\
    case 0x61:  /* NMI status and control register.  Keyboard port. */ \
    case 0x40 ... 0x43: /* Programmable interval timer */ \
    case 0x3f8 ... 0x3ff:  /* COM1 */		  \
    break;

#define IOREQ_READ      1
#define IOREQ_WRITE     0

#define STATE_IOREQ_NONE        0
#define STATE_IOREQ_READY       1
#define STATE_IOREQ_INPROCESS   2
#define STATE_IORESP_READY      3

#define IOREQ_TYPE_PIO          0 /* pio */
#define IOREQ_TYPE_COPY         1 /* mmio ops */
#define IOREQ_TYPE_AND          2
#define IOREQ_TYPE_OR           3
#define IOREQ_TYPE_XOR          4
#define IOREQ_TYPE_XCHG         5
#define IOREQ_TYPE_ADD          6
#define IOREQ_TYPE_TIMEOFFSET   7
#define IOREQ_TYPE_INVALIDATE   8 /* mapcache */
#define IOREQ_TYPE_SUB          9

/*
 * VMExit dispatcher should cooperate with instruction decoder to
 * prepare this structure and notify service OS and DM by sending
 * virq
 */
struct ioreq {
    uint64_t addr;          /*  physical address            */
    uint64_t size;          /*  size in bytes               */
    uint64_t count;         /*  for rep prefixes            */
    uint64_t data;          /*  data (or paddr of data)     */
    uint8_t state:4;
    uint8_t data_is_ptr:1;  /*  if 1, data above is the guest paddr 
                             *   of the real data to use.   */
    uint8_t dir:1;          /*  1=read, 0=write             */
    uint8_t df:1;
    uint8_t pad:1;
    uint8_t type;           /* I/O type                     */
    uint8_t _pad0[6];
    uint64_t io_count;      /* How many IO done on a vcpu   */
};
typedef struct ioreq ioreq_t;

struct irqreq {
    struct {
        uint8_t pending;
        uint32_t irq;
        uint32_t vector;
    } pending;
};

struct vcpu_iodata {
    struct ioreq vp_ioreq;
    struct irqreq vp_irqreq;
};

typedef struct vcpu_iodata vcpu_iodata_t;

struct shared_iopage {
    struct vcpu_iodata   vcpu_iodata[1];
};
typedef struct shared_iopage shared_iopage_t;

struct buf_ioreq {
    uint8_t  type;   /* I/O type                    */
    uint8_t  pad:1;
    uint8_t  dir:1;  /* 1=read, 0=write             */
    uint8_t  size:2; /* 0=>1, 1=>2, 2=>4, 3=>8. If 8, use two buf_ioreqs */
    uint32_t addr:20;/* physical address            */
    uint32_t data;   /* data                        */
};
typedef struct buf_ioreq buf_ioreq_t;

#define IOREQ_BUFFER_SLOT_NUM     511 /* 8 bytes each, plus 2 4-byte indexes */
struct buffered_iopage {
    unsigned int read_pointer;
    unsigned int write_pointer;
    buf_ioreq_t buf_ioreq[IOREQ_BUFFER_SLOT_NUM];
}; /* NB. Size of this structure must be no greater than one page. */
typedef struct buffered_iopage buffered_iopage_t;

struct hvm_io_op {
    unsigned int            instr;      /* instruction */
    unsigned int            flags;
    unsigned long           addr;       /* virt addr for overlap PIO/MMIO */
    struct {
        unsigned int        operand[2]; /* operands */
        unsigned long       immediate;  /* immediate portion */
    };
    L4_IA32_GPRegs_t    *io_context; /* current context */
};


class qemu_dm_t
{
public:
    struct {
        __attribute__((aligned(4096))) shared_iopage_t sio_page;
        __attribute__((aligned(4096))) buffered_iopage_t bio_page;
    } s_pages;

    L4_ThreadId_t qemu_dm_server_id;
    L4_ThreadId_t pager_id;
    L4_ThreadId_t irq_server_id;

    L4_Word_t last_pending_irq;
    L4_Word_t last_pending_vector;

    void init(void);

    struct hvm_io_op mmio_op;

    unsigned char qemu_dm_pager_stack[KB(16)] ALIGNED(CONFIG_STACK_ALIGN);
    void pager_server_loop(void);

    void init_shared_pages(void) {
	memset(&s_pages.sio_page,0,PAGE_SIZE);
	memset(&s_pages.bio_page,0,PAGE_SIZE);
    }

    ioreq_t * get_ioreq_page(void) { return &s_pages.sio_page.vcpu_iodata[0].vp_ioreq; }

    L4_Word_t send_pio(L4_Word_t port, L4_Word_t count, L4_Word_t size,
		       L4_Word_t &value, uint8_t dir, uint8_t df, uint8_t value_is_ptr);

    L4_Word_t send_mmio_req(uint8_t, L4_Word_t, L4_Word_t, L4_Word_t, L4_Word_t, uint8_t, uint8_t, uint8_t);

    void raise_irq(L4_Word_t irq);
    void reraise_irq(L4_Word_t vector);
    bool pending_irq(L4_Word_t &vector, L4_Word_t &irq );

    L4_Word_t raise_event(L4_Word_t event);

};


/*
 *  DEFINITIONS FOR CPU BARRIERS
 */
#if defined(__i386__)
#define mb()  __asm__ __volatile__ ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define rmb() __asm__ __volatile__ ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define wmb() __asm__ __volatile__ ( "" : : : "memory")
#elif defined(__x86_64__)
#define mb()  __asm__ __volatile__ ( "mfence" : : : "memory")
#define rmb() __asm__ __volatile__ ( "lfence" : : : "memory")
#define wmb() __asm__ __volatile__ ( "" : : : "memory")
#elif defined(__ia64__)
#define mb()   __asm__ __volatile__ ("mf" ::: "memory")
#define rmb()  __asm__ __volatile__ ("mf" ::: "memory")
#define wmb()  __asm__ __volatile__ ("mf" ::: "memory")
#elif defined(__powerpc__)
/* XXX loosen these up later */
#define mb()   __asm__ __volatile__ ("sync" : : : "memory")
#define rmb()  __asm__ __volatile__ ("sync" : : : "memory") /* lwsync? */
#define wmb()  __asm__ __volatile__ ("sync" : : : "memory") /* eieio? */
#else
#error "Define barriers"
#endif

#endif /* __QEMU_DM_H__ */

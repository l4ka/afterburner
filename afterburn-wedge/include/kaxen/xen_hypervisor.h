/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/include/kaxen/xen_hypervisor.h
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__XEN_HYPERVISOR_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__XEN_HYPERVISOR_H__

#include INC_ARCH(types.h)

/* Declare types needed by Xen's include files.
 */
typedef s8_t  s8,  int8_t;
typedef u8_t  u8,  uint8_t;
typedef s16_t s16, int16_t;
typedef u16_t u16, uint16_t;
typedef s32_t s32, int32_t;
typedef u32_t u32, uint32_t;
typedef s64_t s64, int64_t;
typedef u64_t u64, uint64_t;

// Includes from Xen's public include tree.
#include <xen.h>
#include <event_channel.h>
#include <dom0_ops.h>
#include <physdev.h>
#ifdef CONFIG_XEN_2_0
#include <io/domain_controller.h>
#else
#include <sched.h>
#include <io/console.h>
#endif

// Afterburn includes.
// TODO (amd64)
#include INC_ARCH(page.h)
#include INC_ARCH(cpu.h)
#include INC_ARCH(cycles.h)
//#include INC_WEDGE(vcpulocal.h)
#include <burn_counters.h>

// TODO (amd64)
// We allocate an address for the shared info within our linker script.  
// Doing so enables us to statically allocate an address for the
// shared info page, without allocating physical memory backing.

#ifdef CONFIG_XEN_2_0

typedef shared_info_t xen_time_info_t;

class xen_shared_info_t : public shared_info_t
{
public:
    u64_t get_cpu_freq(unsigned cpu = 0)
	{ return cpu_freq; }
    unsigned get_num_cpus()
	{ return 1; }
    volatile xen_time_info_t * get_time_info(unsigned cpu = 0)
	{ return this; }
    bool upcall_pending()
	{ return *(volatile u8 *)&vcpu_data[0].evtchn_upcall_pending; }

    u64_t get_current_time_ns( u64_t current_cycles=get_cycles() )
    {
	volatile xen_time_info_t *time_info = get_time_info();
	u64_t time, tsc;
	u32_t time_version;
	do {
	    time_version = time_info->time_version1;
	    time = time_info->system_time; /* nanosecs since boot */
	    tsc = time_info->tsc_timestamp;
	} while( time_version != time_info->time_version2 );
	return time + (current_cycles - tsc) / (get_cpu_freq()/1000000)*1000;
    }
};

#else	/* Xen 3.0 */

typedef vcpu_time_info xen_time_info_t;
typedef shared_info_t xen_wc_info_t;

class xen_shared_info_t : public shared_info_t
{
public:
    volatile xen_time_info_t * get_time_info(unsigned cpu = 0)
	{ return &vcpu_info[cpu].time; }
    volatile xen_wc_info_t * get_wc_info()
	{ return this; }
    volatile vcpu_info_t & get_vcpu_info(unsigned cpu = 0)
	{ return vcpu_info[cpu]; }

    u64_t get_cpu_freq(unsigned cpu = 0)
    {
	u64_t freq = 
	    (1000000000ULL << 32) / vcpu_info[cpu].time.tsc_to_system_mul;
	if( vcpu_info[cpu].time.tsc_shift < 0 )
	    return freq << -vcpu_info[cpu].time.tsc_shift;
	else
	    return freq >> vcpu_info[cpu].time.tsc_shift;
    }

    unsigned get_num_cpus()
	{ return MAX_VIRT_CPUS; }
    bool upcall_pending(unsigned cpu = 0)
	{ return get_vcpu_info(cpu).evtchn_upcall_pending; }
    
#if 0
    volatile u64_t get_current_time_ns( u64_t current_cycles = get_cycles() )
    {
	u64_t time, tsc;
	u32_t time_version;
	volatile xen_time_info_t *time_info = get_time_info();
	do {
	    time_version = time_info->version;
	    time = time_info->system_time; /* nanosecs since boot */
	    tsc = time_info->tsc_timestamp;
	} while( (time_version & 1) | (time_info->version ^ time_version) );
	return time + (current_cycles - tsc) / (get_vcpu().cpu_hz/1000000)*1000;
    }
#endif
};

#endif

extern xen_shared_info_t xen_shared_info;



class xen_start_info_t : public start_info_t
{
public:
    bool is_privileged_dom()  { return flags & SIF_PRIVILEGED; }
    bool is_init_dom()        { return flags & SIF_INITDOMAIN; }
#ifdef CONFIG_XEN_2_0
    bool is_blk_backend_dom() { return flags & SIF_BLK_BE_DOMAIN; }
    bool is_net_backend_dom() { return flags & SIF_NET_BE_DOMAIN; }
#endif
};
extern xen_start_info_t xen_start_info;

// XXX move to separate header?
#ifdef CONFIG_ARCH_IA32

#define XEN_hypercall0(name)                                        \
    ({                                                              \
	word_t __ret;                                               \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret)                                      \
		: "0" (name)                                        \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall1(name, a1)                                    \
    ({                                                              \
	word_t __ign1, __ret;                                       \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=b" (__ign1)                       \
		: "0" (name), "1" (a1)                              \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall2(name, a1, a2)                                \
    ({                                                              \
	word_t __ign1, __ign2, __ret;                               \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=b" (__ign1), "=c" (__ign2)        \
		: "0" (name), "1" (a1), "2" (a2)                    \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall3(name, a1, a2, a3)                            \
    ({                                                              \
	word_t __ign1, __ign2, __ign3, __ret;                       \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=b" (__ign1), "=c" (__ign2),       \
		  "=d" (__ign3)                                     \
		: "0" (name), "1" (a1), "2" (a2), "3" (a3)          \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall4(name, a1, a2, a3, a4)                        \
    ({                                                              \
	word_t __ign1, __ign2, __ign3, __ign4, __ret;               \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=b" (__ign1), "=c" (__ign2),       \
		  "=d" (__ign3), "=S" (__ign4)                      \
		: "0" (name), "1" (a1), "2" (a2), "3" (a3), "4" (a4)\
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })


#elif defined (CONFIG_ARCH_AMD64)

#define XEN_hypercall0(name)                                        \
    ({                                                              \
	word_t __ret;                                               \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret)                                      \
		: "0" (name)                                        \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall1(name, a1)                                    \
    ({                                                              \
	word_t __ign1, __ret;                                       \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=D" (__ign1)                       \
		: "0" (name), "1" (a1)                              \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall2(name, a1, a2)                                \
    ({                                                              \
	word_t __ign1, __ign2, __ret;                               \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=D" (__ign1), "=S" (__ign2)        \
		: "0" (name), "1" (a1), "2" (a2)                    \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall3(name, a1, a2, a3)                            \
    ({                                                              \
	word_t __ign1, __ign2, __ign3, __ret;                       \
	__asm__ __volatile__ ( TRAP_INSTR                           \
		: "=a" (__ret), "=D" (__ign1), "=S" (__ign2),       \
		  "=d" (__ign3)                                     \
		: "0" (name), "1" (a1), "2" (a2), "3" (a3)          \
		: "memory"                                          \
		);                                                  \
	__ret;                                                      \
    })

#define XEN_hypercall4(name, a1, a2, a3, a4)                        \
    ({                                                              \
	word_t __ign1, __ign2, __ign3, __ret;                       \
	__asm__ __volatile__ ( "movq %8,%%r10; "                    \
	                       TRAP_INSTR                           \
		: "=a" (__ret), "=D" (__ign1), "=S" (__ign2),       \
		  "=d" (__ign3)                                     \
		: "0" (name), "1" (a1), "2" (a2), "3" (a3), "g" (a4)\
		: "memory", "r10"                                   \
		);                                                  \
	__ret;                                                      \
    })
#else
#error "Not ported to this architecture!"
#endif

INLINE long XEN_console_io( int cmd, int count, char *buffer )
{
    INC_BURN_COUNTER(XEN_console_io);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall3( __HYPERVISOR_console_io, cmd, count, buffer );
    ADD_PERF_COUNTER(XEN_console_io_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_block( void )
{
    INC_BURN_COUNTER(XEN_sched_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_sched_op, SCHEDOP_block );
    ADD_PERF_COUNTER(XEN_sched_op_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_shutdown( word_t reason )
{
    INC_BURN_COUNTER(XEN_sched_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
#ifdef CONFIG_XEN_2_0
    long ret = XEN_hypercall1( __HYPERVISOR_sched_op, SCHEDOP_shutdown | (reason << SCHEDOP_reasonshift) );
#else
    long ret = XEN_hypercall2( __HYPERVISOR_sched_op, SCHEDOP_shutdown,
	                       reason );
#endif
    ADD_PERF_COUNTER(XEN_sched_op_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_yield( void )
{
    INC_BURN_COUNTER(XEN_sched_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_sched_op, SCHEDOP_yield );
    ADD_PERF_COUNTER(XEN_sched_op_cycles, get_cycles() - cycles);
    return ret;
}

INLINE int XEN_update_va_mapping( word_t addr, word_t val, word_t flags )
{
    INC_BURN_COUNTER(XEN_update_va_mapping);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
#ifdef CONFIG_XEN_2_0
#ifdef CONFIG_ARCH_AMD64
#error "Not ported to this architecture!"
#endif
    addr >>= PAGE_BITS;
    int ret = XEN_hypercall3( __HYPERVISOR_update_va_mapping,
	                      addr, val, flags );
#else
#ifdef CONFIG_ARCH_AMD64
    int ret = XEN_hypercall3( __HYPERVISOR_update_va_mapping,
	                      addr, val, flags);
#else
    int ret = XEN_hypercall4( __HYPERVISOR_update_va_mapping,
	                      addr, val, 0/*PAE*/, flags);
#endif
#endif
    ADD_PERF_COUNTER(XEN_update_va_mapping_cycles, get_cycles() - cycles);
    return ret;
}

INLINE int XEN_mmu_update( mmu_update_t *reqs, unsigned int count,
	unsigned int *success_count, domid_t domain = DOMID_SELF )
{
    INC_BURN_COUNTER(XEN_mmu_update);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    int ret = XEN_hypercall4( __HYPERVISOR_mmu_update, reqs, count,
	                      success_count, domain );
    ADD_PERF_COUNTER(XEN_mmu_update_cycles, get_cycles() - cycles);
    return ret;
}

#ifdef CONFIG_XEN_3_0
INLINE int XEN_mmuext_opt( struct mmuext_op *op, unsigned int count,
	unsigned int *success_count, domid_t domain = DOMID_SELF )
{
    INC_BURN_COUNTER(XEN_mmuext_opt);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    int ret = XEN_hypercall4( __HYPERVISOR_mmuext_op, op, count,
	                      success_count, domain );
    ADD_PERF_COUNTER(XEN_mmuext_opt_cycles, get_cycles() - cycles);
    return ret;
}
#endif

// XXX different interface on amd64
INLINE long XEN_set_callbacks(unsigned long event_selector,
	unsigned long event_address, unsigned long failsafe_selector,
	unsigned long failsafe_address )
{
    INC_BURN_COUNTER(XEN_set_callbacks);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall4( __HYPERVISOR_set_callbacks, event_selector,
	                       event_address, failsafe_selector,
			       failsafe_address );
    ADD_PERF_COUNTER(XEN_set_callbacks_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_event_channel_op( evtchn_op_t *op )
{
    INC_BURN_COUNTER(XEN_event_channel_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_event_channel_op, op );
    ADD_PERF_COUNTER(XEN_event_channel_op_cycles, get_cycles() - cycles);
    return ret;
}

#if 0
INLINE long XEN_event_channel_op_NEW( int cmd, void *op )
{
    INC_BURN_COUNTER(XEN_event_channel_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall2( 32, cmd, op );
    ADD_PERF_COUNTER(XEN_event_channel_op_cycles, get_cycles() - cycles);
    return ret;
}
#endif

INLINE long XEN_set_trap_table( trap_info_t *tbl )
{
    INC_BURN_COUNTER(XEN_set_trap_table);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_set_trap_table, tbl );
    ADD_PERF_COUNTER(XEN_set_trap_table_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_dom0_op( dom0_op_t *dom0_op )
{
    INC_BURN_COUNTER(XEN_dom0_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_dom0_op, dom0_op );
    ADD_PERF_COUNTER(XEN_dom0_op_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_set_timer_op( u64_t time_val )
{
    INC_BURN_COUNTER(XEN_set_timer_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
#ifdef CONFIG_ARCH_IA32
    u32_t *time_val_words = (u32_t *)&time_val;
    long ret = XEN_hypercall2( __HYPERVISOR_set_timer_op,
#ifdef CONFIG_XEN_2_0
	                       time_val_words[1], time_val_words[0]
#else
	                       time_val_words[0], time_val_words[1]
#endif
	                     );
#else
    long ret = XEN_hypercall1( __HYPERVISOR_set_timer_op, time_val );
#endif
    ADD_PERF_COUNTER(XEN_set_timer_op_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_physdev_op( physdev_op_t *op )
{
    INC_BURN_COUNTER(XEN_physdev_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_physdev_op, op );
    ADD_PERF_COUNTER(XEN_physdev_op_cycles, get_cycles() - cycles);
    return ret;
}

// FIXME this is different on amd64
INLINE long XEN_stack_switch( word_t ss, word_t esp )
{
    INC_BURN_COUNTER(XEN_stack_switch);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall2( __HYPERVISOR_stack_switch, ss, esp);
    ADD_PERF_COUNTER(XEN_stack_switch_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_fpu_taskswitch( void )
{
    INC_BURN_COUNTER(XEN_fpu_taskswitch);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall0( __HYPERVISOR_fpu_taskswitch );
    ADD_PERF_COUNTER(XEN_fpu_taskswitch_cycles, get_cycles() - cycles);
    return ret;
}

#if defined(CONFIG_XEN_2_0)
INLINE long XEN_set_fast_trap( int index )
{
    INC_BURN_COUNTER(XEN_set_fast_trap);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_set_fast_trap, index );
    ADD_PERF_COUNTER(XEN_set_fast_trap_cycles, get_cycles() - cycles);
    return ret;
}
#endif

INLINE int XEN_multicall( multicall_entry_t *entries, unsigned int count )
{
    INC_BURN_COUNTER(XEN_multicall);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    int ret = XEN_hypercall2( __HYPERVISOR_multicall, entries, count );
    ADD_PERF_COUNTER(XEN_multicall_cycles, get_cycles() - cycles);
    return ret;
}

// TODO (amd64)
#if 0
INLINE long XEN_update_descriptor( word_t maddr, segdesc_t segdesc )
{
    INC_BURN_COUNTER(XEN_update_descriptor);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret;
    word_t r1, r2, r3;
    __asm__ __volatile__ ( TRAP_INSTR
	    : "=a" (ret), "=b" (r1), "=c" (r2), "=d" (r3)
	    : "0" (__HYPERVISOR_update_descriptor), "1" (maddr),
	      "2" (segdesc.x.words[0]), "3" (segdesc.x.words[1])
	    : "memory"
	    );
    ADD_PERF_COUNTER(XEN_update_descriptor_cycles, get_cycles() - cycles);
    return ret;
}
#endif

INLINE long XEN_set_gdt( word_t *mfn_list, word_t gdt_entry_cnt )
{
    INC_BURN_COUNTER(XEN_set_gdt);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    int ret = XEN_hypercall2( __HYPERVISOR_set_gdt, mfn_list, gdt_entry_cnt);
    ADD_PERF_COUNTER(XEN_set_gdt_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_xen_version( void )
{
    INC_BURN_COUNTER(XEN_xen_version);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_xen_version, 0 );
    ADD_PERF_COUNTER(XEN_xen_version_cycles, get_cycles() - cycles);
    return ret;
}

INLINE long XEN_set_debugreg( int reg, unsigned long value )
{
    INC_BURN_COUNTER(XEN_set_debugreg);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall2( __HYPERVISOR_set_debugreg, reg, value );
    ADD_PERF_COUNTER(XEN_set_debugreg_cycles, get_cycles() - cycles);
    return ret;
}

INLINE unsigned long XEN_get_debugreg( int reg )
{
    INC_BURN_COUNTER(XEN_get_debugreg);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    long ret = XEN_hypercall1( __HYPERVISOR_get_debugreg, reg );
    ADD_PERF_COUNTER(XEN_get_debugreg_cycles, get_cycles() - cycles);
    return ret;
}


// TODO (amd64)
#if 0
INLINE bool xen_do_callbacks()
{ 
    if( xen_shared_info.upcall_pending() ) {
	INC_BURN_COUNTER(xen_upcall_pending);
	XEN_xen_version();
	return true;
    }
    return false;
}
#endif

#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__XEN_HYPERVISOR_H__ */

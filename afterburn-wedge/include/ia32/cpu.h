/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/ia32/cpu.h
 * Description:   The CPU model, and support types.
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

#ifndef __AFTERBURN_WEDGE__INCLUDE__IA32__CPU_H__
#define __AFTERBURN_WEDGE__INCLUDE__IA32__CPU_H__

#include <hiostream.h>
#include <burn_counters.h>

/* TODO:
 *  - Cache pointers in the cpu_t directly to the tss.esp0 and tss.ss0.
 */

#include INC_ARCH(types.h)
#include INC_ARCH(page.h)
#include INC_ARCH(bitops.h)

#define IA32_EXC_DIVIDE_ERROR		0
#define IA32_EXC_DEBUG			1
#define IA32_EXC_NMI			2
#define IA32_EXC_BREAKPOINT		3
#define IA32_EXC_OVERFLOW		4
#define IA32_EXC_BOUNDRANGE		5
#define IA32_EXC_INVALIDOPCODE		6
#define IA32_EXC_NOMATH_COPROC		7
#define IA32_EXC_DOUBLEFAULT		8
#define IA32_EXC_COPSEG_OVERRUN		9
#define IA32_EXC_INVALID_TSS		10
#define IA32_EXC_SEGMENT_NOT_PRESENT	11
#define IA32_EXC_STACKSEG_FAULT		12
#define IA32_EXC_GENERAL_PROTECTION	13
#define IA32_EXC_PAGEFAULT		14
#define IA32_EXC_RESERVED		15
#define IA32_EXC_FPU_FAULT		16
#define IA32_EXC_ALIGNEMENT_CHECK	17
#define IA32_EXC_MACHINE_CHECK		18
#define IA32_EXC_SIMD_FAULT		19

/* Intel reserved exceptions */
#define IA32_EXC_RESERVED_FIRST		20
#define IA32_EXC_RESERVED_LAST		31

#if defined(CONFIG_DEVICE_APIC)
class local_apic_t;
#endif


struct gate_t
{
    union {
	u32_t raw[2];
	struct {
	    u32_t offset_low : 16;
	    u32_t segment_selector : 16;
	    u32_t : 8;
	    u32_t type : 3;
	    u32_t d : 1;
	    u32_t : 1;
	    u32_t dpl : 2;
	    u32_t p : 1;
	    u32_t offset_high : 16;
	} trap;
	struct {
	    u32_t offset_low : 16;
	    u32_t segment_selector : 16;
	    u32_t : 8;
	    u32_t type : 3;
	    u32_t d : 1;
	    u32_t : 1;
	    u32_t dpl : 2;
	    u32_t p : 1;
	    u32_t offset_high : 16;
	} interrupt;
	struct {
	    u32_t : 16;
	    u32_t tss_segment_selector : 16;
	    u32_t : 8;
	    u32_t type : 3;
	    u32_t : 2;
	    u32_t dpl : 2;
	    u32_t p : 1;
	    u32_t : 16;
	} task;
    } x;

    bool is_present()
	{ return 1 == x.trap.p; }
    bool is_trap()
	{ return 7 == x.trap.type; }
    bool is_interrupt()
	{ return 6 == x.interrupt.type; }
    bool is_task()
	{ return 5 == x.task.type; }
    bool is_32bit()
	{ return 1 == x.trap.d; }

    u32_t get_offset()
	{ return (x.trap.offset_high << 16) | (x.trap.offset_low); }
    u32_t get_segment_selector()
	{ return x.trap.segment_selector; }
    u32_t get_privilege_level()
	{ return x.trap.dpl; }
    u32_t get_type()
	{ return x.trap.type; }
};

struct segdesc_t
{
    union {
	u64_t raw;
	u32_t words[2];
	struct {
	    u64_t limit_lower : 16;
	    u64_t base_lower : 24;
	    u64_t accessed : 1;
	    u64_t writable : 1;
	    u64_t expansion_direction : 1;
	    u64_t type : 1;
	    u64_t category : 1;
	    u64_t privilege : 2;
	    u64_t present : 1;
	    u64_t limit_upper : 4;
	    u64_t unused : 1;
	    u64_t : 1;
	    u64_t big: 1;
	    u64_t granularity : 1;
	    u64_t base_upper : 8;
	} data __attribute__((packed));
	struct {
	    u64_t limit_lower : 16;
	    u64_t base_lower : 24;
	    u64_t accessed : 1;
	    u64_t readable : 1;
	    u64_t conforming : 1;
	    u64_t type : 1;
	    u64_t category : 1;
	    u64_t privilege : 2;
	    u64_t present : 1;
	    u64_t limit_upper : 4;
	    u64_t unused : 1;
	    u64_t : 1;
	    u64_t default_bit : 1;
	    u64_t granularity : 1;
	    u64_t base_upper : 8;
	} code __attribute__((packed));
	struct {
	    u64_t limit_lower : 16;
	    u64_t base_lower : 24;
	    u64_t type : 4;
	    u64_t category : 1;
	    u64_t privilege : 2;
	    u64_t present : 1;
	    u64_t limit_upper : 4;
	    u64_t : 1;
	    u64_t : 1;
	    u64_t : 1;
	    u64_t granularity : 1;
	    u64_t base_upper : 8;
	} system __attribute__((packed));
	struct {
	    u64_t limit_lower : 16;
	    u64_t base_lower : 24;
	    u64_t type : 4;
	    u64_t category : 1;
	    u64_t privilege : 2;
	    u64_t present : 1;
	    u64_t limit_upper : 4;
	    u64_t unused : 1;
	    u64_t : 1;
	    u64_t : 1;
	    u64_t granularity : 1;
	    u64_t base_upper : 8;
	} tss __attribute__((packed));
    } x;

    bool is_present()
	{ return 1 == x.data.present; }
    bool is_data()
	{ return (1 == x.data.category) && (0 == x.data.type); }
    bool is_code()
	{ return (1 == x.data.category) && (1 == x.data.type); }
    bool is_system()
	{ return 0 == x.system.category; }

    u32_t get_privilege()
	{ return x.data.privilege; }
    u32_t get_base()
	{ return x.data.base_lower | (x.data.base_upper << 24); }
    u32_t get_limit()
	{ return x.data.limit_lower | (x.data.limit_upper << 16); }
    u32_t get_scale()
	{ return x.data.granularity << 12; }

    void set_base( u32_t base32 )
	{ x.data.base_lower = base32; x.data.base_upper = base32 >> 24; }
    void set_limit( u32_t limit32 )
	{ x.data.limit_lower = limit32 >> 12; 
	    x.data.limit_upper = limit32 >> (16 + 12); }
};

struct dtr_t
{
    static const u32_t operand16_base_mask = 0x00ffffff;
    union {
	u8_t raw[6];
	struct {
    	    u16_t limit;
    	    u32_t base;
       	} __attribute__((packed)) fields;
    } x;

    gate_t * get_gate_table()
	{ return (gate_t *)x.fields.base; }
    u32_t get_total_gates()
	{ return (x.fields.limit+1) / 8; }

    segdesc_t * get_desc_table()
	{ return (segdesc_t *)x.fields.base; }
    u32_t get_total_desc()
	{ return (x.fields.limit+1) / 8; }

};
static const dtr_t dtr_boot = {x: {fields: {limit: 0xffff, base: 0}}};
#if 0
INLINE hiostream_t& operator<< (hiostream_t &ios, dtr_t dtr)
{
    return ios << "base: " << (void *)dtr.x.fields.base
	       << ", limit: " << (void *)(u32_t)dtr.x.fields.limit;
}
#endif

struct cr0_t
{
    union {
	u32_t raw;
	struct {
	    u32_t pe : 1; // Protection Enable
	    u32_t mp : 1; // Monitor Coprocessor
	    u32_t em : 1; // Emulation
	    u32_t ts : 1; // Task Switched
	    u32_t et : 1; // Extension Type
	    u32_t ne : 1; // Numeric Error
	    u32_t : 10;
	    u32_t wp : 1; // Write Protect
	    u32_t : 1;
	    u32_t am : 1; // Alignment Mask
	    u32_t : 10;
	    u32_t nw : 1; // Not Write-thread
	    u32_t cd : 1; // Cache Disable
	    u32_t pg : 1; // Paging
	} fields;
    } x;

    bool protected_mode_enabled() { return x.fields.pe; }
    void enable_protected_mode() { x.fields.pe = 1; } 
    bool real_mode() { return !x.fields.pe; }
    bool paging_enabled() { return x.fields.pg && protected_mode_enabled(); }
    bool write_protect() { return x.fields.wp; }
    bool task_switched() { return x.fields.ts; }
};

static const cr0_t cr0_boot = {x: {raw: 0x60000010}};

#if 0
INLINE hiostream_t& operator<< (hiostream_t &ios, cr0_t &cr0)
{
    return ios << "pe: " << cr0.x.fields.pe
	       << ", mp: " << cr0.x.fields.mp
	       << ", em: " << cr0.x.fields.em
	       << ", ts: " << cr0.x.fields.ts
	       << ", et: " << cr0.x.fields.et
	       << ", ne: " << cr0.x.fields.ne
	       << ", wp: " << cr0.x.fields.wp
	       << ", am: " << cr0.x.fields.am
	       << ", nw: " << cr0.x.fields.nw
	       << ", cd: " << cr0.x.fields.cd
	       << ", pg: " << cr0.x.fields.pg;
}
#endif

typedef u32_t cr2_t;
static const cr2_t cr2_boot = 0;

struct cr3_t
{
    union {
	u32_t raw;
	struct {
	    u32_t : 3;
	    u32_t pwt : 1; // Page-level Writes Transparent
	    u32_t pcd : 1; // Page-level Cache Disable
	    u32_t : 7;
	    u32_t pgdir_base : 20;
	} fields __attribute__((packed));
    } x;

    u32_t get_pdir_addr() { return x.raw & PAGE_MASK; }

};
static const cr3_t cr3_boot = {x: {raw: 0}};

#if 0
INLINE hiostream_t& operator<< (hiostream_t &ios, cr3_t &cr3)
{
    return ios << "pgdir: " << (void *)cr3.get_pdir_addr() 
	       << ", pwt: " << cr3.x.fields.pwt
	       << ", pcd: " << cr3.x.fields.pcd;
}
#endif

struct cr4_t
{
    union {
	u32_t raw;
	struct {
	    u32_t vme : 1; // Virtual-8086 Mode Extensions
	    u32_t pvi : 1; // Protected-Mode Virtual Interrupts
	    u32_t tsd : 1; // Time Stamp Disable
	    u32_t de  : 1; // Debugging Extensions

	    u32_t pse : 1; // Page Size Extensions
	    u32_t pae : 1; // Physical Address Extension
	    u32_t mce : 1; // Machine-Check Enable
	    u32_t pge : 1; // Page Global Enable

	    u32_t pce : 1; // Performance-Monitoring Counter Enable
	    u32_t oxfxsr : 1;
	    u32_t osxmmexcpt : 1;
	    u32_t reserved : 21;
	} fields;
    } x;

    bool is_page_global_enabled()
	{ return x.fields.pge == 1; }
    bool is_pse_enabled()
	{ return x.fields.pse == 1; }
};
static const cr4_t cr4_boot = {x: {raw: 0x90}};

static const u16_t cs_boot = 0xf000;
static const u16_t segment_boot = 0;

static const u32_t flags_user_mask = 0xffc08cff;
static const u32_t flags_system_at_user = 0x00083200;
struct flags_t
{
    static const u32_t fi_bit = 9;

    // TODO: remove the volatile keyword
    volatile union {
	u32_t raw;
	struct {
	    u32_t cf : 1;
	    u32_t    : 1;
	    u32_t pf : 1;
	    u32_t    : 1;

	    u32_t af : 1;
	    u32_t    : 1;
	    u32_t zf : 1;
	    u32_t sf : 1;

	    u32_t tf : 1;	/* system */
	    u32_t fi : 1;	/* system */
	    u32_t df : 1;
	    u32_t of : 1;

	    u32_t iopl : 2;	/* system */
	    u32_t nt : 1;	/* system */
	    u32_t    : 1;

	    u32_t rf : 1;	/* system */
	    u32_t vm : 1;	/* system */
	    u32_t ac : 1;	/* system */
	    u32_t vif : 1;	/* system */

	    u32_t vip : 1;	/* system */
	    u32_t id : 1;	/* system */
	    u32_t    : 10;
	} fields;
    } x;

    void prepare_for_gate( gate_t &gate ) {
	x.fields.tf = x.fields.vm = x.fields.rf = x.fields.nt = 0;
	if( gate.is_interrupt() )
	    x.fields.fi = 0;
    }

    u32_t get_user_flags()
	{ return x.raw & flags_user_mask; }
    u32_t get_system_flags()
	{ return x.raw & ~flags_user_mask; }

    word_t get_iopl()
	{ return x.fields.iopl; }
    void set_iopl( word_t iopl )
	{ x.fields.iopl = iopl; }

    bool interrupts_enabled()
	{ return x.fields.fi; }

    u32_t get_raw()
	{ return x.raw; }
};
static const flags_t flags_boot = {x: {raw: 2}};

struct tss_t	// task-state segment
{
    u16_t previous_task_link;	// dynamic
    u16_t reserved1;
    u32_t esp0;	// static
    u16_t ss0;	// static
    u16_t reserved2;
    u32_t esp1;	// static
    u16_t ss1;	// static
    u16_t reserved3;
    u32_t esp2;	// static
    u16_t ss2;	// static
    u16_t reserved4;

    cr3_t cr3;	// static
    u32_t eip;	// dynamic
    u32_t eflags;	// dynamic
    u32_t eax;	// dynamic
    u32_t ecx;	// dynamic
    u32_t edx;	// dynamic
    u32_t ebx;	// dynamic
    u32_t esp;	// dynamic
    u32_t ebp;	// dynamic
    u32_t esi;	// dynamic
    u32_t edi;	// dynamic

    u16_t es;	// dynamic
    u16_t reserved5;
    u16_t cs;	// dynamic
    u16_t reserved6;
    u16_t ss;	// dynamic
    u16_t reserved7;
    u16_t ds;	// dynamic
    u16_t reserved8;
    u16_t fs;	// dynamic
    u16_t reserved9;
    u16_t gs;	// dynamic
    u16_t reserved10;
    u16_t ldt_seg_sel;	// static
    u16_t reserved11;
    u16_t debug_trap : 1;	// static
    u16_t   : 15;
    u16_t io_map_addr;	// static
} __attribute__((packed));


struct iret_frame_t
{
    u32_t ip;
    u32_t cs;
    flags_t flags;
};

struct iret_redirect_frame_t
{
    word_t stack_remove;
    word_t ip;
    word_t cs;
    flags_t flags;
};

struct lret_frame_t
{
    u32_t ip;
    u32_t cs;
};

struct iret_user_frame_t
{
    u32_t ip;
    u32_t cs;
    flags_t flags;
    u32_t sp;
    u32_t ss;
};

struct cpu_t 
{
    flags_t flags;	// 0
    word_t cs;		// 4
    word_t ss;		// 8
    word_t tss_base;	// 12
    u16_t tss_seg;	// 16
    dtr_t idtr;		// 18
    word_t ds;		// 24

    word_t es;		// 28
    word_t fs;		// 32
    word_t gs;		// 36
 
    cr2_t cr2;		// 40
    cr3_t cr3;		// 44
    cr4_t cr4;		// 48

    u16_t ldtr;		// 52
    dtr_t gdtr;		// 56

    cr0_t cr0;		// 60

    word_t redirect;	// 64
    u32_t dr[8];	// 68
    			// 100

#if defined(CONFIG_DEVICE_APIC)
    local_apic_t *lapic;	// 104
    void set_lapic(local_apic_t *l) 
	{ lapic = l; }
    local_apic_t *get_lapic() 
	{ return lapic; }
#else
    word_t dummy;		// 104
#endif
  
    void enable_protected_mode() { cr0.enable_protected_mode(); }

    bool interrupts_enabled()
	{ return flags.x.fields.fi; }
    bool disable_interrupts()
	{ return bit_test_and_clear_atomic( flags_t::fi_bit, flags.x.raw ); }
    void restore_interrupts( bool old_state );

    tss_t *get_tss()
	{ return (tss_t *)tss_base; }
    tss_t *get_tss( u16_t segment );
    bool segment_exists( u16_t segment );

    static u32_t get_segment_privilege( u16_t segment )
	{ return segment & 3; }

    u32_t get_privilege()
	{ return get_segment_privilege(cs); }

    void validate_interrupt_redirect();
    void prepare_interrupt_redirect()
	{
	    if( !interrupts_enabled() )
		return;
	    INC_BURN_COUNTER(cpu_vector_redirect);
	    extern word_t burn_interrupt_redirect[];
	    // gcc is pushing %ebp on the stack frame,
	    // even though we compile without frame pointers.
    	    word_t *ret_address = (word_t *)__builtin_frame_address(0) + 1;
	    redirect = *ret_address;
#if defined(CONFIG_DEBUGGER)
	    validate_interrupt_redirect();
#endif
	    *ret_address = word_t(burn_interrupt_redirect);
	    return;
	    /* gcc has a bug: __builtin_frame_address(0) is relative to
	     * the frame pointer (%ebp), even when we compile with full
	     * optimizations and disable the frame pointer.  I have noticed
	     * that gcc generates proper code if I include a reference to
	     * __builtin_frame_address(1).  So the f1ollowing line causes
	     * gcc to emit correct code.
	     */
	    __asm__ __volatile__ (");" : : "r"(__builtin_frame_address(1)));
	}
};
extern cpu_t cpu_boot;

struct frame_t
{
    union {
	u32_t raw[9];
	struct {
	    // This ordering is chosen to match the IA32 ModR/M Byte
	    // decoding.  Thus when decoding an instruction, the bit pattern
	    // is useable for a direct lookup in this register list.
	    // Additionally, this order is hard coded in the afterburn
	    // assembler macros.
	    u32_t eax;
	    u32_t ecx;
	    u32_t edx;
	    u32_t ebx;
	    u32_t esp;
	    u32_t ebp;
	    u32_t esi;
	    u32_t edi;
	    flags_t flags;
	} fields;
    } x;
};

struct user_frame_t
{
    iret_user_frame_t *iret;
    frame_t *regs;
};

struct iret_handler_frame_t
{
    frame_t frame;
    iret_frame_t redirect_iret;
    iret_user_frame_t iret;
};


class pgent_t
{
public:
    bool is_valid()
	{ return 1 == x.fields.present; }
    bool is_writable()
	{ return 1 == x.fields.rw; }
    bool is_readable()
	{ return true; }
    bool is_executable()
	{ return true; }
    bool is_accessed()
	{ return 1 == x.fields.accessed; }
    bool is_dirty()
	{ return 1 == x.fields.dirty; }
    bool is_executed()
	{ return is_accessed(); }
    bool is_superpage()
	{ return 1 == x.fields.size; }
    bool is_kernel()
	{ return 0 == x.fields.privilege; }
    bool is_global()
	{ return 1 == x.fields.global; }
    void set_address(u32_t new_addr)
        { x.raw = (new_addr & PAGE_MASK) | (x.raw & (~PAGE_MASK)); }


#if defined(CONFIG_WEDGE_KAXEN)
    bool is_valid_xen_machine()
	{ return (x.raw & (valid | xen_machine)) == (valid | xen_machine); }
    bool is_xen_machine()
	{ return x.raw & xen_machine; }
    bool is_xen_special()
	{ return x.raw & xen_special; }
#if defined(CONFIG_KAXEN_UNLINK_HEURISTICS) || defined(CONFIG_KAXEN_WRITABLE_PGTAB)
    bool is_xen_ptab_valid()
	{ return is_valid() || is_xen_machine(); }
    bool is_xen_unlinked()
	{ return !is_valid() && is_xen_machine(); }
    void set_xen_unlinked()
	{ set_invalid(); }
    void set_xen_linked()
	{ set_valid(); }
#else
    bool is_xen_unlinked() { return false; }
#endif

    void xen_reset()
	{ x.raw |= valid | writable; x.raw &= ~(xen_machine | xen_special); }
    void set_xen_machine()
	{ x.fields.xen_machine = 1; }
    void set_xen_special()
	{ x.fields.xen_special = 1; }
    void clear_xen_special()
	{ x.fields.xen_special = 0; }
    void clear_xen_machine()
	{ x.fields.xen_machine = 0; clear_xen_special(); }
#endif


    u32_t get_address()
	{ return x.raw & PAGE_MASK; }
    pgent_t * get_ptab()
	{ return (pgent_t *)get_address(); }
    u32_t get_raw()
	{ return x.raw; }

    void clear()
	{ x.raw = 0; }
    void set_writable()
	{ x.fields.rw = 1; }
    void set_read_only()
	{ x.fields.rw = 0; }
    void set_accessed( u32_t flag )
	{ x.fields.accessed = flag; }
    void set_dirty( u32_t flag)
	{ x.fields.dirty = flag; }
    void set_kernel()
	{ x.fields.privilege = 0; }
    void set_user()
	{ x.fields.privilege = 1; }
    void set_global()
	{ x.fields.global = 1; }
    void clear_global()
	{ x.fields.global = 0; }
    void set_valid()
	{ x.fields.present = 1; }
    void set_invalid()
	{ x.fields.present = 0; }

    static u32_t get_pdir_idx( u32_t addr )
	{ return addr >> PAGEDIR_BITS; }
    static u32_t get_ptab_idx( u32_t addr )
	{ return (addr & ~PAGEDIR_MASK) >> PAGE_BITS; }

    bool operator == (const pgent_t & pgent)
        { return x.raw == pgent.x.raw; }
    pgent_t& operator= (const u32_t raw)
	{ x.raw = raw; return *this; }
    pgent_t( word_t raw )
	{ x.raw = raw; }
    pgent_t() {}

public:
    enum pagesize_t {
	size_4k = 0,
	size_4m = 1
    };

    enum fields_t {
	valid		= (1 << 0),
	writable	= (1 << 1),
	privilege	= (1 << 2),
	write_through	= (1 << 3),
	cache_disabled	= (1 << 4),
	accessed	= (1 << 5),
	dirty		= (1 << 6),
	size		= (1 << 7),
	global		= (1 << 8),
#if defined(CONFIG_WEDGE_KAXEN)
	xen_special	= (1 << 9),
	xen_machine	= (1 << 11),
#endif
    };

private:
    union {
	struct {
	    u32_t present		:1;
	    u32_t rw			:1;
	    u32_t privilege		:1;
	    u32_t write_through		:1;

	    u32_t cache_disabled	:1;
	    u32_t accessed		:1;
	    u32_t dirty			:1;
	    u32_t size			:1;

	    u32_t global		:1;
#if defined(CONFIG_WEDGE_KAXEN)
	    // Xen internally uses bit 10, see xen/arch/x86/x86_32/domain_page.c
	    u32_t xen_special		:1;
	    u32_t xen_internal		:1;
	    u32_t xen_machine		:1;
#else
	    u32_t unused		:3;
#endif

	    u32_t base			:20;
	} fields;
	u32_t raw;
    } x;
};

struct pgfault_err_t {
    union {
	struct {
	    word_t valid : 1;
	    word_t write : 1;
	    word_t user  : 1;
	    word_t rsvd  : 1;
	} fields;
	word_t raw;
    } x;

    bool is_valid()
	{ return x.fields.valid; }
    bool is_write_fault()
	{ return x.fields.write; }
    bool is_user_fault()
	{ return x.fields.user; }
    bool is_reserved_fault()
	{ return x.fields.rsvd; }
};

struct burn_clobbers_frame_t {
    word_t burn_ret_address;
    word_t frame_pointer;
    word_t edx;
    word_t ecx;
    word_t eax;
    word_t guest_ret_address;
    word_t params[0];
};

struct burn_frame_t {
    word_t burn_ret_address;
    word_t frame_pointer;
    frame_t regs;
    word_t guest_ret_address;
    word_t params[0];
};

struct burn_iret_redirect_frame_t {
    struct {
	word_t edx;
	word_t ecx;
	word_t eax;
    } clobbers;
    iret_frame_t redirect;
    iret_user_frame_t iret;
};

struct burn_redirect_frame_t {
    struct {
	word_t edx;
	word_t ecx;
	word_t eax;
    } clobbers;
    union {
	struct {
	    iret_frame_t unused;
	    word_t ip;
	} in;
	struct {
	    word_t ip;
	    iret_frame_t iret;
	} out_redirect;
	struct {
	    iret_frame_t iret;
	    word_t ip;
	} out_no_redirect;
    } x;

    bool is_redirect()
	{ return x.out_no_redirect.iret.ip != 0; }
    void do_redirect( word_t vector );
    bool do_redirect();
};


class ia32_fpu_t
{
public:
    static void init()
	{ __asm__ __volatile__ ("finit\n"); }

    static void save_state(word_t *fpu_state)
        {
	    __asm__ __volatile__ (
		"fxsave %0"
		: 
		: "m" (*(u32_t*)fpu_state));
	}

    static void load_state(word_t *fpu_state)
	{
	    __asm__ __volatile__ (
		"fxrstor %0"
	    : 
	    : "m" (*(u32_t*)fpu_state));
	}

    static const word_t fpu_state_size = 512;
    static const word_t fpu_state_words = 512 / sizeof(word_t);
    
};

/*
 * global functions
 */
extern bool frontend_init( cpu_t * cpu );

INLINE void cpu_read_cpuid(frame_t *frame, u32_t &max_basic, u32_t &max_extended)
{
    u32_t func = frame->x.fields.eax;

    // Note: cpuid is a serializing instruction!

    if( max_basic == 0 )
    {
	// We need to determine the maximum inputs that this CPU permits.

	// Query for the max basic input.
	frame->x.fields.eax = 0;
	__asm__ __volatile__ ("cpuid"
			      : "=a"(frame->x.fields.eax), "=b"(frame->x.fields.ebx), 
				"=c"(frame->x.fields.ecx), "=d"(frame->x.fields.edx)
			      : "0"(frame->x.fields.eax));
	
	max_basic = frame->x.fields.eax;

	// Query for the max extended input.
	frame->x.fields.eax = 0x80000000;
	__asm__ __volatile__ ("cpuid"
			      : "=a"(frame->x.fields.eax), "=b"(frame->x.fields.ebx),
				"=c"(frame->x.fields.ecx), "=d"(frame->x.fields.edx)
			      : "0"(frame->x.fields.eax));
	max_extended = frame->x.fields.eax;

	// Restore the original request.
	frame->x.fields.eax = func;

	// XXX this seems to be necessary for xen?
	if( max_basic == 0 )
		max_basic = 3;
    }
    // TODO: constrain basic functions to 3 if 
    // IA32_CR_MISC_ENABLES.BOOT_NT4 (bit 22) is set.

    // Execute the cpuid request.
    __asm__ __volatile__ ("cpuid"
			  : "=a"(frame->x.fields.eax), "=b"(frame->x.fields.ebx), 
			    "=c"(frame->x.fields.ecx), "=d"(frame->x.fields.edx)
			  : "0"(frame->x.fields.eax));

}

#endif	/* __AFTERBURN_WEDGE__INCLUDE__IA32__CPU_H__ */

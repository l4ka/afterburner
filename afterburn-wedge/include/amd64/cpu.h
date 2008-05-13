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

#ifndef __AFTERBURN_WEDGE__INCLUDE__AMD64__CPU_H__
#define __AFTERBURN_WEDGE__INCLUDE__AMD64__CPU_H__

#include <hiostream.h>
#include <burn_counters.h>

/* TODO:
 *  - Cache pointers in the cpu_t directly to the tss.esp0 and tss.ss0.
 */

#include INC_ARCH(types.h)
#include INC_ARCH(page.h)
#include INC_ARCH(bitops.h)

#define AMD64_EXC_DIVIDE_ERROR		0
#define AMD64_EXC_DEBUG			1
#define AMD64_EXC_NMI			2
#define AMD64_EXC_BREAKPOINT		3
#define AMD64_EXC_OVERFLOW		4
#define AMD64_EXC_BOUNDRANGE		5
#define AMD64_EXC_INVALIDOPCODE		6
#define AMD64_EXC_NOMATH_COPROC		7
#define AMD64_EXC_DOUBLEFAULT		8
#define AMD64_EXC_COPSEG_OVERRUN		9
#define AMD64_EXC_INVALID_TSS		10
#define AMD64_EXC_SEGMENT_NOT_PRESENT	11
#define AMD64_EXC_STACKSEG_FAULT		12
#define AMD64_EXC_GENERAL_PROTECTION	13
#define AMD64_EXC_PAGEFAULT		14
#define AMD64_EXC_RESERVED		15
#define AMD64_EXC_FPU_FAULT		16
#define AMD64_EXC_ALIGNEMENT_CHECK	17
#define AMD64_EXC_MACHINE_CHECK		18
#define AMD64_EXC_SIMD_FAULT		19

/* Intel reserved exceptions */
#define AMD64_EXC_RESERVED_FIRST		20
#define AMD64_EXC_RESERVED_LAST		31

#if defined(CONFIG_DEVICE_APIC)
class local_apic_t;
#endif


struct gate_t
{
    union {
	word_t raw[2];
	struct {
	    word_t offset_low : 16;
	    word_t segment_selector : 16;
	    word_t ist : 5;
	    word_t : 5;
	    word_t type : 4;
	    word_t mbz : 1;
	    word_t dpl : 2;
	    word_t p : 1;
	    word_t offset_mid : 16;
	    word_t offset_high : 32;
	    word_t : 32;
	} trap __attribute__((packed));
	struct {
	    word_t offset_low : 16;
	    word_t segment_selector : 16;
	    word_t ist : 5;
	    word_t : 5;
	    word_t type : 4;
	    word_t mbz : 1;
	    word_t dpl : 2;
	    word_t p : 1;
	    word_t offset_mid : 16;
	    word_t offset_high : 32;
	    word_t : 32;
	} interrupt __attribute__((packed));
    } x;

    bool is_present()
	{ return 1 == x.trap.p; }
    bool is_trap()
	{ return 7 == x.trap.type; }
    bool is_interrupt()
	{ return 6 == x.interrupt.type; }

    word_t get_offset()
	{ return (x.trap.offset_high << 32) | (x.trap.offset_mid << 16)
	                                    | (x.trap.offset_low); }
    word_t get_segment_selector()
	{ return x.trap.segment_selector; }
    word_t get_privilege_level()
	{ return x.trap.dpl; }
    word_t get_type()
	{ return x.trap.type; }
};

struct segdesc_t
{
    // almost all fields are ignored
    union {
	word_t raw;
	struct {
	    // note that the base is only used for address calculations on
	    // the fs and gs registers, and there are special msrs to set
	    // the base..
	    word_t : 16; // limit lower - ignored
	    word_t base_lower : 24; // base lower - ignored for ds, es, ss
	    word_t : 7;  // a, r, c, t, c, privilege - ignored
	    word_t present : 1;
	    word_t : 8;  // segment upper, avl, unused, db, g - ignored
	    word_t base_upper : 8;
	} data __attribute__((packed));
	struct {
	    word_t : 40; // limit lower, base lower - ignored
	    word_t : 2;  // r, a - ignored
	    word_t conforming : 1;
	    word_t : 2;  // part of the type - ignored
	    word_t privilege : 2;
	    word_t present : 1;
	    word_t : 5;  // limit upper, avl - ignored
	    word_t long_mode : 1;
	    word_t default_bit : 1;
	    word_t : 9;  // g, base upper - ignored
	} code __attribute__((packed));
    } x;

    bool is_present()
	{ return 1 == x.data.present; }

    word_t get_privilege()
	{ return x.code.privilege; }
    word_t get_base()
	{ return x.data.base_lower | (x.data.base_upper << 24); }

    void set_base( word_t base32 )
	{ x.data.base_lower = base32; x.data.base_upper = base32 >> 24; }
};

struct sysdesc_t
{
    union {
	word_t raw[2];
	struct {
	    word_t limit_lower : 16;
	    word_t base_lower : 24;
	    word_t type : 4;
	    word_t mbz : 1;
	    word_t privilege : 2;
	    word_t present : 1;
	    word_t limit_upper : 4;
	    word_t avl : 1;
	    word_t : 2;
	    word_t granularity : 1;
	    word_t base_upper : 40;
	    word_t : 8;
	    word_t mbz2 : 5;
	    word_t : 19;
	} fields __attribute__((packed));
    } x;

    bool is_present()
	{ return 1 == x.fields.present; }
    bool is_ldt()
	{ return 0x2 == x.fields.type; }
    bool is_avl_tss()
	{ return 0x9 == x.fields.type; }
    bool is_busy_tss()
	{ return 0xb == x.fields.type; }
    bool is_call_gate()
	{ return 0xc == x.fields.type; }
    bool is_int_gate()
	{ return 0xe == x.fields.type; }
    bool is_trap_gate()
	{ return 0xf == x.fields.type; }

    gate_t* as_gate()
	{ return (gate_t *)this; }

    word_t get_privilege()
	{ return x.fields.privilege; }
    word_t get_base()
	{ return x.fields.base_lower | (x.fields.base_upper << 24); }
    word_t get_limit()
	{ return x.fields.limit_lower | (x.fields.limit_upper << 16); }

    void set_base( word_t base64 )
	{ x.fields.base_lower = base64; x.fields.base_upper = base64 >> 24; }
    void set_limit( word_t limit32 )
	{ x.fields.limit_lower = limit32; x.fields.limit_upper = limit32 >> 16; }
};

struct dtr_t
{
    static const u32_t operand16_base_mask = 0x00ffffff; // XXX ???
    union {
	u8_t raw[8];
	struct {
    	    u16_t limit;
    	    word_t base;
       	} __attribute__((packed)) fields;
    } x;

    sysdesc_t * get_gate_table()
	{ return (sysdesc_t *)x.fields.base; }
    word_t get_total_gates()
	{ return (x.fields.limit+1) / sizeof( sysdesc_t ); }

    segdesc_t * get_desc_table()
	{ return (segdesc_t *)x.fields.base; }
    u32_t get_total_desc()
	{ return (x.fields.limit+1) / sizeof( segdesc_t ); }
};
static const dtr_t dtr_boot = {x: {fields: {limit: 0xffff, base: 0}}};

INLINE hiostream_t& operator<< (hiostream_t &ios, dtr_t dtr)
{
    return ios << "base: " << (void *)dtr.x.fields.base
	       << ", limit: " << (void *)(u32_t)dtr.x.fields.limit;
}

struct cr0_t
{
    union {
	word_t raw;
	struct {
	    word_t pe : 1; // Protection Enable
	    word_t mp : 1; // Monitor Coprocessor
	    word_t em : 1; // Emulation
	    word_t ts : 1; // Task Switched
	    word_t et : 1; // Extension Type
	    word_t ne : 1; // Numeric Error
	    word_t : 10;
	    word_t wp : 1; // Write Protect
	    word_t : 1;
	    word_t am : 1; // Alignment Mask
	    word_t : 10;
	    word_t nw : 1; // Not Write-through
	    word_t cd : 1; // Cache Disable
	    word_t pg : 1; // Paging
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

typedef word_t cr2_t;
static const cr2_t cr2_boot = 0;

struct cr3_t
{
    union {
	word_t raw;
	struct {
	    word_t : 3;
	    word_t pwt : 1; // Page-level Writes Transparent
	    word_t pcd : 1; // Page-level Cache Disable
	    word_t : 7;
	    word_t pgdir_base : 40;
	    word_t : 12;
	} fields __attribute__((packed));
    } x;

    word_t get_pdir_addr() { return x.raw & PAGE_MASK; }

};
static const cr3_t cr3_boot = {x: {raw: 0}};

INLINE hiostream_t& operator<< (hiostream_t &ios, cr3_t &cr3)
{
    return ios << "pgdir: " << (void *)cr3.get_pdir_addr() 
	       << ", pwt: " << cr3.x.fields.pwt
	       << ", pcd: " << cr3.x.fields.pcd;
}

struct cr4_t
{
    union {
	word_t raw;
	struct {
	    word_t vme : 1; // Virtual-8086 Mode Extensions
	    word_t pvi : 1; // Protected-Mode Virtual Interrupts
	    word_t tsd : 1; // Time Stamp Disable
	    word_t de  : 1; // Debugging Extensions

	    word_t pse : 1; // Page Size Extensions
	    word_t pae : 1; // Physical Address Extension
	    word_t mce : 1; // Machine-Check Enable
	    word_t pge : 1; // Page Global Enable

	    word_t pce : 1; // Performance-Monitoring Counter Enable
	    word_t oxfxsr : 1;
	    word_t osxmmexcpt : 1;
	    word_t reserved : 51;
	} fields;
    } x;

    bool is_page_global_enabled()
	{ return x.fields.pge == 1; }
    bool is_pse_enabled()
	{ return x.fields.pse == 1; }
};
static const cr4_t cr4_boot = {x: {raw: 0x90}};

// XXX ?
static const u16_t cs_boot = 0xf000;
static const u16_t segment_boot = 0;

// XXX ???
static const word_t flags_user_mask = 0xffc08cff;
static const word_t flags_system_at_user = 0x00083200;
struct flags_t
{
    static const word_t fi_bit = 9;

    // TODO: remove the volatile keyword
    volatile union {
	word_t raw;
	struct {
	    word_t cf : 1;
	    word_t    : 1;
	    word_t pf : 1;
	    word_t    : 1;

	    word_t af : 1;
	    word_t    : 1;
	    word_t zf : 1;
	    word_t sf : 1;

	    word_t tf : 1;	/* system */
	    word_t fi : 1;	/* system */
	    word_t df : 1;
	    word_t of : 1;

	    word_t iopl : 2;	/* system */
	    word_t nt : 1;	/* system */
	    word_t    : 1;

	    word_t rf : 1;	/* system */
	    word_t vm : 1;	/* system */
	    word_t ac : 1;	/* system */
	    word_t vif : 1;	/* system */

	    word_t vip : 1;	/* system */
	    word_t id : 1;	/* system */
	    word_t    : 42;
	} fields;
    } x;

    // XXX ???
    void prepare_for_gate( gate_t &gate ) {
	x.fields.tf = x.fields.vm = x.fields.rf = x.fields.nt = 0;
	if( gate.is_interrupt() )
	    x.fields.fi = 0;
    }

    word_t get_user_flags()
	{ return x.raw & flags_user_mask; }
    word_t get_system_flags()
	{ return x.raw & ~flags_user_mask; }

    word_t get_iopl()
	{ return x.fields.iopl; }
    void set_iopl( word_t iopl )
	{ x.fields.iopl = iopl; }

    bool interrupts_enabled()
	{ return x.fields.fi; }

    word_t get_raw()
	{ return x.raw; }
};
static const flags_t flags_boot = {x: {raw: 2}};

struct tss_t	// task-state segment
{
    word_t : 32;
    word_t rsp0 : 64;
    word_t rsp1 : 64;
    word_t rsp2 : 64;
    word_t : 64;
    word_t ist1 : 64;
    word_t ist2 : 64;
    word_t ist3 : 64;
    word_t ist4 : 64;
    word_t ist5 : 64;
    word_t ist6 : 64;
    word_t ist7 : 64;
    word_t : 64;
    word_t : 16;
    word_t io_map_addr : 16;
} __attribute__((packed));


// same privilege level
struct iret_frame_t
{
    word_t ip;
    word_t cs;
    flags_t flags;
    word_t sp;
    word_t ss;
};

// XXX ???
#if 0
struct iret_redirect_frame_t
{
    word_t stack_remove;
    word_t ip;
    word_t cs;
    flags_t flags;
};
#endif

// should not be necessary on amd64
#if 0
struct lret_frame_t
{
    u32_t ip;
    u32_t cs;
};
#endif

// higher privilege level
struct iret_user_frame_t
{
    word_t ip;
    word_t cs;
    flags_t flags;
    word_t sp;
    word_t ss;
};

struct cpu_t 
{
    flags_t flags;
    word_t cs;	
    word_t ss;	
    word_t tss_base;
    u16_t tss_seg;
    dtr_t idtr;	
    word_t ds;	

    word_t es;	
    word_t fs;	
    word_t gs;	
 
    cr2_t cr2;	
    cr3_t cr3;	
    cr4_t cr4;	

    u16_t ldtr;	
    dtr_t gdtr;	

    cr0_t cr0;	

    // XXX ????
    word_t redirect;
    word_t dr[8];

    // XXX add efer?
    word_t syscall_entry;
    
    /*
     * We compress pending IRQ vectors in that cluster each bit represents 8
     * vectors. 
     * 
     */
    word_t irq_vectors;

    void set_irq_vector(word_t vector)
	{ bit_set_atomic((vector >> 3), irq_vectors); }
    void clear_irq_vector(word_t vector) 
	{ bit_clear_atomic((vector >> 3), irq_vectors); }
    word_t get_irq_vectors() 
	{ return irq_vectors; }



#if defined(CONFIG_DEVICE_APIC)
    local_apic_t *lapic;
    void set_lapic(local_apic_t *l) 
	{ lapic = l; }
    local_apic_t *get_lapic() 
	{ return lapic; }
#else
    word_t dummy;	
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

    // XXX ????
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
	    __asm__ __volatile__ (";" : : "r"(__builtin_frame_address(1)));
	}
};
extern cpu_t cpu_boot;

struct frame_t
{
    union {
	word_t raw[9];
	struct {
	    // This ordering is chosen to match the AMD64 ModR/M Byte
	    // decoding.  Thus when decoding an instruction, the bit pattern
	    // is useable for a direct lookup in this register list.
	    // Additionally, this order is hard coded in the afterburn
	    // assembler macros.
	    word_t rax;
	    word_t rcx;
	    word_t rdx;
	    word_t rbx;
	    word_t rsp;
	    word_t rbp;
	    word_t rsi;
	    word_t rdi;
	    word_t r8;
	    word_t r9;
	    word_t r10;
	    word_t r11;
	    word_t r12;
	    word_t r13;
	    word_t r14;
	    word_t r15;
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
	// FIXME nx bit!
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
    void set_address(word_t new_addr)
	// TODO make sure new_addr fits
        { x.fields.base = new_addr >> PAGE_BITS; }


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


    word_t get_address()
	{ return x.fields.base << PAGE_BITS; }
    pgent_t * get_ptab()
	{ return (pgent_t *)get_address(); }
    word_t get_raw()
	{ return x.raw; }

    void clear()
	{ x.raw = 0; }
    void set_writable()
	{ x.fields.rw = 1; }
    void set_read_only()
	{ x.fields.rw = 0; }
    void set_accessed( word_t flag )
	{ x.fields.accessed = flag; }
    void set_dirty( word_t flag)
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

    static word_t get_pml4_idx( word_t addr )
	{ return (addr & ~CANONICAL_MASK) >> PML4_BITS; }
    static word_t get_pdp_idx( word_t addr )
	{ return (addr & ~PML4_MASK) >> PDP_BITS; }
    static word_t get_pdir_idx( word_t addr )
	{ return (addr & ~PDP_MASK) >> PAGEDIR_BITS; }
    static word_t get_ptab_idx( word_t addr )
	{ return (addr & ~PAGEDIR_MASK) >> PAGE_BITS; }

    bool operator == (const pgent_t & pgent)
        { return x.raw == pgent.x.raw; }
    pgent_t( word_t raw )
	{ x.raw = raw; }
    pgent_t() {}
    pgent_t& operator= (const word_t raw)
	{ x.raw = raw; return *this; }

public:
    enum pagesize_t {
	size_4k = 0,
	size_2m = 1
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
	nx		= (1ul << 63)
    };

private:
    union {
	struct __attribute__((__packed__)) {
	    word_t present		:1;
	    word_t rw			:1;
	    word_t privilege		:1;
	    word_t write_through	:1;
	    word_t cache_disabled	:1;
	    word_t accessed		:1;
	    word_t dirty		:1;
	    word_t size			:1; // FIXME? also pat
	    word_t global		:1;
#if defined(CONFIG_WEDGE_KAXEN)
	    // Xen internally uses bit 10, see xen/arch/x86/x86_32/domain_page.c
	    // XXX FIXME does that also hold true for amd64?
	    word_t xen_special		:1;
	    word_t xen_internal		:1;
	    word_t xen_machine		:1;
#else
	    word_t unused		:3;
#endif

	    word_t base			:40;
	    word_t unused2		:11; // XXX xen uses some of these
	    word_t nx			:1;
	} fields;
	word_t raw;
    } x;
};

struct pgfault_err_t {
    union {
	struct {
	    word_t valid : 1;
	    word_t write : 1;
	    word_t user  : 1;
	    word_t rsvd  : 1;
	    word_t instr : 1;
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
    bool is_instruction_fetch_fault()
    	{ return x.fields.instr; }
};

struct burn_clobbers_frame_t {
    word_t burn_ret_address;
    word_t frame_pointer;

    word_t rax;
    word_t rcx;
    word_t rdx;
    word_t rsi;
    word_t rdi;
    word_t r8;
    word_t r9;
    word_t r10;
    word_t r11;

    word_t guest_ret_address;
    word_t params[0];
};

#if 0
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
#endif


#if 0
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
#endif

/*
 * global functions
 */

extern bool frontend_init( cpu_t * cpu );
 

#endif	/* __AFTERBURN_WEDGE__INCLUDE__AMD64__CPU_H__ */

#ifndef __ARCH__IA32__VMX_VMCS_GS_TYPES_H__
#define __ARCH__IA32__VMX_VMCS_GS_TYPES_H__

typedef void* addr_t;

class vmcs_entryctr_iif_t {
public:
    enum vmcs_interruption_e{
	ext_int     = 0,
	hw_nmi      = 2,
	hw_except   = 3,
	sw_intr     = 4,
	priv_except = 5,
	sw_except   = 6,
    };

    enum vector_e {
	de=0, db=1, nmi=2, bp=3, of=4, br=5, ud=6, nm=7, df=8,
	copseg_overrun=9, ts=10, np=11, ss=12, gp=13,
	pf=14, /* int 15 */ mf=16, ac=17, mc=18, xf=19,
    };

public:
    union {
	u32_t raw;
	struct {
	    word_t vector               :  8;
	    vmcs_interruption_e type    :  3;
	    bool  deliver_err_code      :  1;
	    u32_t reserved              : 19;
	    bool valid                  :  1;
	} __attribute__((packed));
    };
};

class vmcs_exectr_cpubased_t {
public:
    union {
	u32_t raw;
	struct {
	    s32_t res0          : 2;
	    u32_t iw            : 1;
	    u32_t tscoff        : 1;
	    u32_t res1          : 3;
	    u32_t hlt           : 1;
	    s32_t res2          : 1;
	    u32_t invplg        : 1;
	    u32_t mwait         : 1;
	    u32_t rdpmc         : 1;
	    u32_t rdtsc         : 1;
	    s32_t res3          : 6;
	    u32_t lcr8          : 1;
	    u32_t scr8          : 1;
	    u32_t tpr_shadow    : 1;
	    s32_t res4          : 1;
	    u32_t movdr         : 1;
	    u32_t io            : 1;
	    u32_t iobitm        : 1;
	    s32_t res5          : 2;
	    u32_t msrbitm	: 1;
	    u32_t monitor       : 1;
	    u32_t pause         : 1;
	    s32_t res6          : 1;
	} __attribute__((packed));
    };
};

class vmcs_gs_ias_t {
public:
    union {
	u32_t raw;
	struct {
	    u32_t bl_sti        :  1;
	    u32_t bl_movss      :  1;
	    u32_t bl_smi        :  1;
	    u32_t bl_nmi        :  1;
	    u32_t mbz0          : 28;
	} __attribute__((packed));
    } ;
};


class vmcs_gs_as_t{
public:
    enum vmcs_gs_as_e {
	active = 0UL, hlt = 1UL, shutdown = 2UL, wf_ipi = 3UL,
    };

public:
    union {
	u32_t raw;
	vmcs_gs_as_e state;
    };
};

class vmcs_gs_rflags_t
{
public:
    union {
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

	    word_t tf : 1;       /* system */
	    word_t fi : 1;       /* system */
	    word_t df : 1;
	    word_t of : 1;

	    word_t iopl : 2;     /* system */
	    word_t nt : 1;       /* system */
	    word_t    : 1;

	    word_t rf : 1;       /* system */
	    word_t vm : 1;       /* system */
	    word_t ac : 1;       /* system */
	    word_t vif : 1;      /* system */

	    word_t vip : 1;      /* system */
	    word_t id : 1;       /* system */
	    word_t    : 10;
	 };
    };

public:
    bool interrupts_enabled (void)
	{ return fi; }
};


class vmcs_gs_pend_dbg_except_t {
public:
    void init ()
	{ mbz0 = mbz1 = mbz2 = 0; }

public:
    union {
	u64_t raw;
	struct {
	    u64_t bx    :  4;
	    u64_t mbz0  :  8;
	    u64_t br    :  1;
	    u64_t mbz1  :  1;
	    u64_t bs    :  1;
	    u64_t mbz2  : 49;
	} __attribute__((packed));
    };
};


class vmcs_ei_qual_t {
public:
    enum soucre_of_tss_e {
	call            = 0,
	iret            = 1,
	jmp             = 2,
	task_gate       = 3,
    };

    enum mem_reg_e {
	mem = 0, reg = 1
    };

    enum gpr_e {
	rax= 0, rcx = 1, rdx = 2, rbx = 3, rsp = 4, rbp = 5, rsi = 6, rdi = 7,
	r8 = 8, r9 = 9, r10 = 10, r11 = 11, r12 = 12, r13 = 13, r14 = 14,
	r15 = 15,
    };

    enum soa_e {
	u8 = 0, u16 = 1, u32 = 3,
    };

    enum access_type_e {
	to_cr = 0, from_cr = 1, clts = 2, lmsw = 3
    };

    enum dir_e {
	out = 0, in = 1,
    };

    enum op_encoding_e {
	dx = 0, immediate = 1,
    };

public:
    union {
	u32_t raw;

	// Page Fault Address.
	addr_t faddr;

	struct {
	    u32_t bx            :  4;
	    u32_t mbz0          :  9;
	    u32_t bd            :  1;
	    u32_t bs            :  1;
	    u32_t res           : 17;
	} __attribute__((packed)) dbg;
	struct {
	    u32_t		selector      : 16;
	    u32_t		mbz0          : 14;
	    soucre_of_tss_e	source        :  2;   /* source_of_tss_e */
	} __attribute__((packed)) task_sw;
	struct {
	    u32_t		cr_num        :  4;
	    access_type_e	access_type   :  2;
	    mem_reg_e		lmsw_op_type  :  1;   /* mem_reg_e */
	    u32_t		mbz0          :  1;
	    gpr_e		mov_cr_gpr    :  4;   /* gpr_e */
	    u32_t		mbz1          :  4;
	    u32_t		lmsw_src_data : 16;
	} __attribute__((packed)) cr_access;
	struct {
	    u32_t		dbg_reg_no    :  3;
	    u32_t		mbz0          :  1;
	    u32_t		dir           :  1;
	    u32_t		mbz1          :  3;
	    gpr_e		mov_dr_gpr    :  4;   /* gpr_e */
	    u32_t		mbz2	      : 20;
	} __attribute__((packed)) mov_dr;
	struct {
	    soa_e		soa	      :  3;   /* soa_e */
	    u32_t		dir           :  1;   /* dir_e */
	    bool		string_instr  :  1;
	    bool		rep_prefixed  :  1;
	    op_encoding_e	op_encoding   :  1;   /* op encoding_e */
	    u32_t		mbz0          :  9;
	    u32_t		port_no       : 16;
	} __attribute__((packed)) io;
    };
};


#endif /* !__ARCH__IA32__VMX_VMCS_GS_TYPES_H__ */

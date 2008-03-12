/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     l4ka/hvm_vmx.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __L4KA__HVM_VMX_H__
#define __L4KA__HVM_VMX_H__

enum hvm_vmx_reason_e 
{
    hvm_vmx_reason_exp_nmi      =  0,
    hvm_vmx_reason_ext_int      =  1,
    hvm_vmx_reason_tf           =  2,
    hvm_vmx_reason_init         =  3,
    hvm_vmx_reason_sipi         =  4,
    hvm_vmx_reason_smi          =  6,
    hvm_vmx_reason_iw           =  7,
    hvm_vmx_reason_tasksw       =  9,
    hvm_vmx_reason_cpuid        = 10,
    hvm_vmx_reason_hlt          = 12,
    hvm_vmx_reason_invd         = 13,
    hvm_vmx_reason_invlpg       = 14,
    hvm_vmx_reason_rdpmc        = 15,
    hvm_vmx_reason_rdtsc        = 16,
    hvm_vmx_reason_rsm          = 17,
    hvm_vmx_reason_vmcall       = 18,
    hvm_vmx_reason_vmclear      = 19,
    hvm_vmx_reason_vmlaunch     = 20,
    hvm_vmx_reason_vmptrld      = 21,
    hvm_vmx_reason_vmptrst      = 22,
    hvm_vmx_reason_vmread       = 23,
    hvm_vmx_reason_vmresume     = 24,
    hvm_vmx_reason_vmwrite      = 25,
    hvm_vmx_reason_vmxoff       = 26,
    hvm_vmx_reason_vmxon        = 27,
    hvm_vmx_reason_cr           = 28,
    hvm_vmx_reason_dr           = 29,
    hvm_vmx_reason_io           = 30,
    hvm_vmx_reason_rdmsr        = 31,
    hvm_vmx_reason_wrmsr        = 32,
    hvm_vmx_reason_entry_invg   = 33,
    hvm_vmx_reason_entry_msrld  = 34,
    hvm_vmx_reason_mwait        = 36,
    hvm_vmx_reason_monitor      = 39,
    hvm_vmx_reason_pause        = 40,
    hvm_vmx_reason_entry_mce    = 41,
    NUM_BE
};
enum hvm_vmx_instr_err_e 
{
    ie_vmcall       =  1,
    ie_vmclear1     =  2,
    ie_vmclear2     =  3,
    ie_vmlaunch     =  4,
    ie_vmresume1    =  5,
    ie_vmresume2    =  6,
    ie_vmentry1     =  7,
    ie_vmentry2     =  8,
    ie_vmptrld1     =  9,
    ie_vmptrld2     = 10,
    ie_vmptrld3     = 11,
    ie_vmrdwr       = 12,
    ie_vmwr         = 13,
    ie_vmentry3     = 26,
};


class hvm_vmx_ei_qual_t {
public:
    hvm_vmx_ei_qual_t ()
	{ raw = 0; }

public:
    enum source_of_tss_e {
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
	word_t raw;

	// Page Fault Address.
	word_t faddr;

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
	    source_of_tss_e	source        :  2;   /* source_of_tss_e */
	} __attribute__((packed)) task_sw;
	struct {
	    u32_t		cr_num        :  4;
	    access_type_e	access_type   :  2;
	    mem_reg_e		lmsw_op_type  :  1;   /* mem_reg_e */
	    u32_t		mbz0          :  1;
	    gpr_e		mov_cr_gpr    :  4;   /* gpr_e */
	    u32_t		mbz1          :  4;
	    u32_t		lmsw_src_data : 16;
	} __attribute__((packed)) mov_cr;
	struct {
	    u32_t		dr_num        :  3;
	    u32_t		mbz0          :  1;
	    u32_t		dir           :  1;
	    u32_t		mbz1          :  3;
	    gpr_e		mov_dr_gpr    :  4;   /* gpr_e */
	    u32_t		mbz2	      : 20;
	} __attribute__((packed)) mov_dr;
	struct {
	    soa_e		soa	      :  3;   /* soa_e */
	    dir_e		dir           :  1;   /* dir_e */
	    bool		string        :  1;
	    bool		rep           :  1;
	    op_encoding_e	op_encoding   :  1;   /* op_encoding_e */
	    u32_t		mbz0          :  9;
	    u32_t		port_num      : 16;
	} __attribute__((packed)) io;
    };
};

class hvm_vmx_gs_ias_t {
public:
    hvm_vmx_gs_ias_t ()
	{ raw = 0; }

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


class hvm_vmx_gs_as_t 
{
public:
    hvm_vmx_gs_as_t ()
	{ raw = 0; }

public:
    enum hvm_vmx_gs_as_e {
	active = 0UL, hlt = 1UL, shutdown = 2UL, wf_ipi = 3UL,
    };

public:
    union {
	u32_t raw;
	hvm_vmx_gs_as_e state;
    };
};


class hvm_vmx_gs_pend_dbg_except_t {
public:
    hvm_vmx_gs_pend_dbg_except_t ()
	{ raw = 0; }

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


class hvm_vmx_int_t {
public:
    hvm_vmx_int_t ()
	{ raw = 0; }

public:
    enum int_type_e {
	ext_int   = 0,
	hw_nmi    = 2,
	hw_except = 3,
	sw_int    = 4,
	sw_prvlg  = 5,
	sw_except = 6,
    };

public:
    union {
	u32_t raw;
	struct {
	    u32_t vector		:  8;
	    int_type_e type		:  3;   /* info_int_type_e */
	    bool err_code_valid		:  1;
	    bool nmi_unblock		:  1;
	    u32_t mbz0			: 18;
	    bool valid			:  1;
	} __attribute__((packed));
    };
};




#endif /* !__L4KA__HVM_VMX_H__ */

#ifndef L4KAVT_VMX_TYPES_H_
#define L4KAVT_VMX_TYPES_H_

#include <l4/types.h>
#include INC_ARCH(cpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(vt/vmx_vmcs_types.h)
#include INC_WEDGE(vt/bitfield.h)

enum L4_BasicExitReason_e {
    EXCEPTION_OR_NMI = 0,
    EXT_INT	     = 1,
    TRIPLE_FAULT     = 2,
    INIT	     = 3,
    SIPI	     = 4,
    SMI		     = 5,
    OTHER_SMI	     = 6,
    IW		     = 7,
    TASK_SWITCH	     = 9,
    CPUID	     = 10,
    HLT		     = 12,
    INVD	     = 13,
    INVLPG	     = 14,
    RDPMC	     = 15,
    RDTSC	     = 16,
    RSM		     = 17,
    VMCALL	     = 18,
    VMCLEAR	     = 19,
    VMLAUNCH	     = 20,
    VMPTRLD	     = 21,
    VMPTRST	     = 22,
    VMREAD	     = 23,
    VMRESUME	     = 24,
    VMWRITE	     = 25,
    VMXOFF	     = 26,
    VMXON	     = 27,
    CR		     = 28,
    DR		     = 29,
    IO		     = 30,
    RDMSR	     = 31,
    WRMSR	     = 32,
};
#define L4_NUM_BASIC_EXIT_REASONS (33)

/* VCPU fields/indices which can be accessed via the virtual fault protocol */
enum L4_VCPUField_e {
    EAX /* EAX */,
    EBX /* EBX */,
    ECX /* ECX */,
    EDX /* EDX */,
    EBP /* EBP */,
    ESI /* ESI */,
    EDI /* EDI */,
    EINFO_EXIT_REASON,
    EINFO_EXIT_QUAL,
    EINFO_EXIT_INSTR_LEN,
    EINFO_GUEST_LIN_ADDR,
    ENTRY_IIF,
    ENTRY_EXCEP_ERR_CODE,
    G_INTR_ABILITY_STATE,
    G_ACTIVITY_STATE,
    CPU_BASED_VM_EXEC_CTRL,
    G_RSP,
    G_RIP,
    G_RFLAGS,
    G_ES_SEL,
    G_CS_SEL,
    G_SS_SEL,
    G_DS_SEL,
    G_FS_SEL,
    G_GS_SEL,
    G_TR_SEL,
    G_LDTR_SEL,
    G_ES_LIMIT,
    G_CS_LIMIT,
    G_SS_LIMIT,
    G_DS_LIMIT,
    G_FS_LIMIT,
    G_GS_LIMIT,
    G_TR_LIMIT,
    G_LDTR_LIMIT,
    G_GDTR_LIMIT,
    G_IDTR_LIMIT,
    G_ES_ATTR,
    G_CS_ATTR,
    G_SS_ATTR,
    G_DS_ATTR,
    G_FS_ATTR,
    G_GS_ATTR,
    G_TR_ATTR,
    G_LDTR_ATTR,
    G_ES_BASE,
    G_CS_BASE,
    G_SS_BASE,
    G_DS_BASE,
    G_FS_BASE,
    G_GS_BASE,
    G_TR_BASE,
    G_LDTR_BASE,
    G_GDTR_BASE,
    G_IDTR_BASE,
    G_SYSENTER_CS,
    G_SYSENTER_ESP,
    G_SYSENTER_EIP,
    PIN_BASED_VM_EXEC_CTRL,
    EXCEPTION_BITMAP,
    TPR_THRESHOLD,
    EXIT_CTRL,
    EXIT_MSR_ST_COUNT,
    EXIT_MSR_LD_COUNT,
    EINFO_VM_INSTR_ERR,
    EINFO_INTERRUPTION_INFO,
    EINFO_INTERRUPTION_ERR_CODE,
    EINFO_IDT_VECT_INFO_FIELD,
    EINFO_IDT_VECT_ERROR_CODE,
    EINFO_INSTR_INFO,
    ENTRY_CTRL,
    ENTRY_MSR_LD_COUNT,
    ENTRY_INSTR_LEN,
    G_CR0,
    G_CR3,
    G_CR4,
    G_DR7,
    G_PEND_DBG_EXCEPT,
    CR0_MASK,
    CR4_MASK,
    CR0_SHADOW,
    CR4_SHADOW,
    G_VMCS_LINK_PTR_F,
    G_VMCS_LINK_PTR_H,
    G_DEBUGCTL_F,
    G_DEBUGCTL_H,
    IO_BITMAP_A_F,
    IO_BITMAP_A_H,
    IO_BITMAP_B_F,
    IO_BITMAP_B_H,
    TSC_OFF_F,
    TSC_OFF_H,
    VAPIC_PAGE_ADDR_F,
    VAPIC_PAGE_ADDR_H,
    EXIT_MSR_ST_ADDR_F,
    EXIT_MSR_ST_ADDR_H,
    EXIT_MSR_LD_ADDR_F,
    EXIT_MSR_LD_ADDR_H,
    ENTRY_MSR_LD_ADDR_F,
    ENTRY_MSR_LD_ADDR_H,
};
#define L4_NUM_VCPU_FIELDS (100)


class ias_t {
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

typedef union {
    L4_Word32_t raw;
    L4_Word32_t ex;

    struct {
	union {
	    L4_Word16_t x;
	    struct {
		L4_Word8_t l;
		L4_Word8_t h;
	    };
	};
	L4_Word16_t __;
    };
} gpr_t;


/* VCPU state type corresponding to the VCPU fields in L4_VCPUField_e */
typedef union {
    struct {
	gpr_t eax;
	gpr_t ebx;
	gpr_t ecx;
	gpr_t edx;
	L4_Word_t ebp;
	L4_Word_t esi;
	L4_Word_t edi;
	L4_Word_t einfo_exit_reason;
	vmcs_ei_qual_t einfo_exit_qual;
	L4_Word_t einfo_exit_instr_len;
	L4_Word_t einfo_guest_lin_addr;
	vmcs_entryctr_iif_t entry_iif;
	L4_Word_t entry_excep_err_code;
	ias_t g_intr_ability_state;
	L4_Word_t g_activity_state;
	vmcs_exectr_cpubased_t cpu_based_vm_exec_ctrl;
	L4_Word_t g_rsp;
	L4_Word_t g_rip;
	vmcs_gs_rflags_t g_rflags;
	L4_Word_t g_es_sel;
	L4_Word_t g_cs_sel;
	L4_Word_t g_ss_sel;
	L4_Word_t g_ds_sel;
	L4_Word_t g_fs_sel;
	L4_Word_t g_gs_sel;
	L4_Word_t g_tr_sel;
	L4_Word_t g_ldtr_sel;
	L4_Word_t g_es_limit;
	L4_Word_t g_cs_limit;
	L4_Word_t g_ss_limit;
	L4_Word_t g_ds_limit;
	L4_Word_t g_fs_limit;
	L4_Word_t g_gs_limit;
	L4_Word_t g_tr_limit;
	L4_Word_t g_ldtr_limit;
	L4_Word_t g_gdtr_limit;
	L4_Word_t g_idtr_limit;
	L4_Word_t g_es_attr;
	L4_Word_t g_cs_attr;
	L4_Word_t g_ss_attr;
	L4_Word_t g_ds_attr;
	L4_Word_t g_fs_attr;
	L4_Word_t g_gs_attr;
	L4_Word_t g_tr_attr;
	L4_Word_t g_ldtr_attr;
	L4_Word_t g_es_base;
	L4_Word_t g_cs_base;
	L4_Word_t g_ss_base;
	L4_Word_t g_ds_base;
	L4_Word_t g_fs_base;
	L4_Word_t g_gs_base;
	L4_Word_t g_tr_base;
	L4_Word_t g_ldtr_base;
	L4_Word_t g_gdtr_base;
	L4_Word_t g_idtr_base;
	L4_Word_t g_sysenter_cs;
	L4_Word_t g_sysenter_esp;
	L4_Word_t g_sysenter_eip;
	L4_Word_t pin_based_vm_exec_ctrl;
	L4_Word_t exception_bitmap;
	L4_Word_t tpr_threshold;
	L4_Word_t exit_ctrl;
	L4_Word_t exit_msr_st_count;
	L4_Word_t exit_msr_ld_count;
	L4_Word_t einfo_vm_instr_err;
	L4_Word_t einfo_interruption_info;
	L4_Word_t einfo_interruption_err_code;
	L4_Word_t einfo_idt_vect_info_field;
	L4_Word_t einfo_idt_vect_error_code;
	L4_Word_t einfo_instr_info;
	L4_Word_t entry_ctrl;
	L4_Word_t entry_msr_ld_count;
	L4_Word_t entry_instr_len;
	L4_Word_t g_cr0;
	L4_Word_t g_cr3;
	L4_Word_t g_cr4;
	L4_Word_t g_dr7;
	L4_Word_t g_pend_dbg_except;
	L4_Word_t cr0_mask;
	L4_Word_t cr4_mask;
	L4_Word_t cr0_shadow;
	L4_Word_t cr4_shadow;
	L4_Word_t g_vmcs_link_ptr_f;
	L4_Word_t g_vmcs_link_ptr_h;
	L4_Word_t g_debugctl_f;
	L4_Word_t g_debugctl_h;
	L4_Word_t io_bitmap_a_f;
	L4_Word_t io_bitmap_a_h;
	L4_Word_t io_bitmap_b_f;
	L4_Word_t io_bitmap_b_h;
	L4_Word_t tsc_off_f;
	L4_Word_t tsc_off_h;
	L4_Word_t vapic_page_addr_f;
	L4_Word_t vapic_page_addr_h;
	L4_Word_t exit_msr_st_addr_f;
	L4_Word_t exit_msr_st_addr_h;
	L4_Word_t exit_msr_ld_addr_f;
	L4_Word_t exit_msr_ld_addr_h;
	L4_Word_t entry_msr_ld_addr_f;
    };

    L4_Word_t raw[L4_NUM_VCPU_FIELDS];
} L4_VCPURegisterSet_t;


typedef bitfield_t<L4_NUM_VCPU_FIELDS> L4_VCPUMaskBitmap_t;

enum L4_VCPUProtocolItemType_e {
    // write state
    SET_MUL_ITEM		= 0,
    SET_REG_ITEM		= 2,
    SET_MSR_ITEM		= 3,
    SET_FLT_ITEM		= 127,

    // read state
    GET_MUL_ITEM		= 128,
    GET_GRP_ITEM		= 129,
    GET_REG_ITEM		= 130,
    GET_MSR_ITEM		= 131,
};


class L4_VCPUProtocolItem_t {
public:
    
    virtual L4_Word_t write( L4_Word_t *table );
    virtual L4_Word_t writeMR( L4_Word_t mr );

    union {
	L4_Word_t raw;

	// Set/Get-Multiple
	struct {
	    L4_BITFIELD3(L4_Word_t,
		    type	:  8,
		    header	:  8,
		    zero	:  16);
	} multiple __attribute__((packed));

	// Set/Get-Single
	struct {
	    L4_BITFIELD2(L4_Word_t,
		    type	: 8,
		    idx	        : 24);

	} single __attribute__((packed));

	// Set/Get-MSR
	struct {
	    L4_BITFIELD2(L4_Word_t,
		    type	: 8,
		    zero	: 24);
	} msr __attribute__((packed));

	// Set/Get-Fault
	struct {
	    L4_BITFIELD3(L4_Word_t,
		    type	:  8,
		    header	:  8,
		    reason	: 16);
	} fault __attribute__((packed));
	
	struct {
	    L4_BITFIELD2(L4_Word_t,
		    type     : 8,
		    reserved : 24);
	} X __attribute__((packed));
    };
};

INLINE L4_Word_t L4_VCPUProtocolItem_t::write( L4_Word_t *table )
{
    table[0] = this->raw;
    return 1;
}

INLINE L4_Word_t L4_VCPUProtocolItem_t::writeMR( L4_Word_t mr )
{
    L4_LoadMR( mr, this->raw );
    return 1;
}

class L4_SetMultipleItem_t : public L4_VCPUProtocolItem_t {
public:
    L4_SetMultipleItem_t( void );
    
    void clear( void );
    void set( L4_VCPUField_e field, L4_Word_t val );
    virtual L4_Word_t write( L4_Word_t *table );
    virtual L4_Word_t writeMR( L4_Word_t mr );

private:
    L4_VCPUMaskBitmap_t bitmap;
    L4_Word_t register_values[L4_NUM_VCPU_FIELDS];
};

INLINE L4_SetMultipleItem_t::L4_SetMultipleItem_t( void )
{
    this->multiple.type = SET_MUL_ITEM;
    this->clear();
}


INLINE void L4_SetMultipleItem_t::clear( void )
{
    this->bitmap.clear();
}

INLINE void L4_SetMultipleItem_t::set( L4_VCPUField_e field, L4_Word_t val )
{
    this->register_values[ field ] = val;
    this->bitmap.setBit( field );
}

// write item registers to table
// return number of written words
INLINE L4_Word_t L4_SetMultipleItem_t::write( L4_Word_t *table )
{
    L4_Word_t idx = 1;
    // reset header
    this->multiple.header = 0;
    
    // write bitmap and set header accordingly
    for( L4_Word_t i = 0; i < this->bitmap.getMaxSizeInWords(); ++i ) {
	// only add part i of bitmap if != 0
	if( this->bitmap.getRaw( i ) != 0 ) {
	    table[idx++] = this->bitmap.getRaw( i );
	    this->multiple.header |= (1 << i);
	}
    }
    	    
    // write registers
    for( L4_Word_t i = 0; i < this->bitmap.getSizeInBits(); ++i ) {
	if( this->bitmap.isSet( i ) ) {
	    table[idx++] = this->register_values[ i ];
	}
    }
    
    table[0] = this->raw;
    
    return idx;
}

// write item registers to message registers beginnig at message register mr
// return number of written words
INLINE L4_Word_t L4_SetMultipleItem_t::writeMR( L4_Word_t mr ) 
{
    L4_Word_t idx = mr+1;
    // reset header
    this->multiple.header = 0;
    
    // write bitmap and set header accordingly
    for( L4_Word_t i = 0; i < this->bitmap.getMaxSizeInWords(); ++i ) {
	// only add part i of bitmap if != 0	
	if( this->bitmap.getRaw( i ) != 0 ) {
	    ASSERT( idx < __L4_NUM_MRS );
	    L4_LoadMR( idx++, this->bitmap.getRaw( i ) );
	    this->multiple.header |= (1 << i);
	}
    }
    	    
    // write registers
    for( L4_Word_t i = 0; i < this->bitmap.getSizeInBits(); ++i ) {
	if( this->bitmap.isSet( i ) ) {
	    ASSERT( idx < __L4_NUM_MRS );	    
	    L4_LoadMR( idx++, this->register_values[ i ] );
	}
    }
    
    // write item tag
    L4_LoadMR( mr, this->raw );
    
    return idx - mr;
}

class L4_GetMultipleItem_t : public L4_VCPUProtocolItem_t {
public:
    L4_GetMultipleItem_t( void );
    L4_GetMultipleItem_t( L4_VCPUMaskBitmap_t _bitmap );
    
    void clear( void );
    void set( L4_VCPUField_e field );
    virtual L4_Word_t write( L4_Word_t *table );
    virtual L4_Word_t writeMR( L4_Word_t mr );

private:
    L4_VCPUMaskBitmap_t bitmap;
};

INLINE L4_GetMultipleItem_t::L4_GetMultipleItem_t( void )
{
    this->multiple.type = GET_MUL_ITEM;
    this->clear();
}

INLINE L4_GetMultipleItem_t::L4_GetMultipleItem_t( L4_VCPUMaskBitmap_t _bitmap )
    : bitmap( _bitmap )
{
    this->multiple.type = GET_MUL_ITEM;
}


INLINE void L4_GetMultipleItem_t::clear( void )
{
    this->bitmap.clear();
}

INLINE void L4_GetMultipleItem_t::set( L4_VCPUField_e field )
{
    this->bitmap.setBit( field );
}

// write item registers to table
// return number of written words
INLINE L4_Word_t L4_GetMultipleItem_t::write( L4_Word_t *table )
{
    L4_Word_t idx = 1;
    // reset header
    this->multiple.header = 0;
    
    // write bitmap and set header accordingly
    for( L4_Word_t i = 0; i < this->bitmap.getMaxSizeInWords(); ++i ) {
	// only add part i of bitmap if != 0
	if( this->bitmap.getRaw( i ) != 0 ) {
	    table[idx++] = this->bitmap.getRaw( i );
	    this->multiple.header |= (1 << i);
	}
    }
    
    table[0] = this->raw;
    
    return idx;
}

// write item registers to message registers beginnig at message register mr
// return number of written words
INLINE L4_Word_t L4_GetMultipleItem_t::writeMR( L4_Word_t mr ) 
{
    L4_Word_t idx = mr+1;
    // reset header
    this->multiple.header = 0;
    
    // write bitmap and set header accordingly
    for( L4_Word_t i = 0; i < this->bitmap.getMaxSizeInWords(); ++i ) {
	// only add part i of bitmap if != 0	
	if( this->bitmap.getRaw( i ) != 0 ) {
	    ASSERT( idx < __L4_NUM_MRS );
	    L4_LoadMR( idx++, this->bitmap.getRaw( i ) );
	    this->multiple.header |= (1 << i);
	}
    }
    	    
    // write item tag
    L4_LoadMR( mr, this->raw );
    
    return idx - mr;
}

class L4_VirtFaultReply_t {
    L4_Word_t typed_items[__L4_NUM_MRS];
    L4_Word_t set_items[__L4_NUM_MRS];
    L4_Word_t get_items[__L4_NUM_MRS];
    
    L4_Word_t num_typed_items;
    L4_Word_t num_set_items;
    L4_Word_t num_get_items;

public:
    void clear( void );
    void add( L4_VCPUProtocolItem_t &item );
    void add( L4_MapItem_t &item );
    void add( L4_GrantItem_t &item );
    void writeMR( void );
};

INLINE void L4_VirtFaultReply_t::clear( void )
{
    this->num_typed_items = 0;
    this->num_set_items = 0;
    this->num_get_items = 0;
}

INLINE void L4_VirtFaultReply_t::add( L4_VCPUProtocolItem_t &item )
{
    switch( (L4_VCPUProtocolItemType_e) item.X.type ) {
	case SET_MUL_ITEM:
        case SET_FLT_ITEM:
	case SET_MSR_ITEM:
	case SET_REG_ITEM:
	    this->num_set_items += item.write( this->set_items + this->num_set_items );
	    ASSERT( this->num_set_items < __L4_NUM_MRS );
//	    con << "added item type " << item.X.type <<", now " << this->num_set_items << " set items\n";
	    break;
	    
	case GET_MUL_ITEM:
	case GET_MSR_ITEM:
	case GET_REG_ITEM:
	    this->num_get_items += item.write( this->get_items + this->num_get_items );
	    ASSERT( this->num_set_items < __L4_NUM_MRS );
//	    con << "added item type " << item.X.type <<", now " << this->num_get_items << " get items\n";	    
	    break;
	    
	default:
	    UNIMPLEMENTED();
    }
}

INLINE void L4_VirtFaultReply_t::add( L4_MapItem_t &mapitem )
{
    UNIMPLEMENTED();
}

INLINE void L4_VirtFaultReply_t::add( L4_GrantItem_t &mapitem )
{
    UNIMPLEMENTED();
}

INLINE void L4_VirtFaultReply_t::writeMR( void )
{
    L4_Word_t mr = 1;

    // load typed items
    for( L4_Word_t i = 0; i < this->num_typed_items; ++i )
	L4_LoadMR( mr++, this->typed_items[i] );

    // load set items
    for( L4_Word_t i = 0; i < this->num_set_items; ++i )
	L4_LoadMR( mr++, this->set_items[i] );

    // load get items
    for( L4_Word_t i = 0; i < this->num_get_items; ++i )
	L4_LoadMR( mr++, this->get_items[i] );
    
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.t = this->num_typed_items;
    tag.X.u = this->num_set_items + this->num_get_items;

    L4_Set_MsgTag( tag );
}

#endif // L4KAVT_VMX_TYPES_H_

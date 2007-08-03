/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     l4ka/rewrite_stackless.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __L4KA__REWRITE_STACKLESS_H__
#define __L4KA__REWRITE_STACKLESS_H__


static inline u8_t *newops_pushf(u8_t *newops) 
{
    newops = op_pushf( newops ); // Push the unprivileged flags.
    newops = op_call( newops, (u8_t **)burn_pushf );
    return newops;
}

static inline u8_t *newops_mov_toseg(u8_t *newops, ia32_modrm_t modrm, u8_t *suffixes) 
{
    if( modrm.is_register_mode() )
	newops = push_reg( newops, modrm.get_rm() );
    else {
	// We can't push the modrm, because it is a 16-bit operand.
	newops = push_reg( newops, OP_REG_EAX );  // Preserve
	newops = insert_operand_size_prefix( newops );  // 16-bit
	newops = mov_from_modrm( newops, OP_REG_EAX, modrm, suffixes );
	newops = push_reg( newops, OP_REG_EAX );
    }
    switch( modrm.get_reg() ) {
	case 0:
	    newops = op_call( newops, (u8_t **)burn_write_es );
	    break;
	case 1:
	    newops = op_call( newops, (u8_t **)burn_write_cs );
	    break;
	case 2:
	    newops = op_call( newops, (u8_t **)burn_write_ss );
	    break;
	case 3:
	    newops = op_call( newops, (u8_t **)burn_write_ds );
	    break;
	case 4:
	    newops = op_call( newops, (u8_t **)burn_write_fs );
	    break;
	case 5:
	    newops = op_call( newops, (u8_t **)burn_write_gs );
	    break;
	default:
	    con << "unknown segment @ " << (u8_t **)newops << '\n';
	    DEBUGGER_ENTER();
	    break;
    }
    newops = clean_stack( newops, 4 );
    if( !modrm.is_register_mode() )
	newops = pop_reg( newops, OP_REG_EAX );  // Restore
    return newops;
}

static inline u8_t *newops_pushcs(u8_t *newops)
{
    newops = push_reg( newops, OP_REG_EAX ); // Create stack space.
    newops = op_call( newops, (u8_t **)burn_read_cs );
    return newops;
}

static inline u8_t *newops_pushss(u8_t *newops)
{
    newops = push_reg( newops, OP_REG_EAX ); // Create stack space.
    newops = op_call( newops, (u8_t **)burn_read_ss );
    return newops;
}

static inline u8_t *newops_popss(u8_t *newops)
{
    newops = op_call( newops, (u8_t **)burn_write_ss );
    newops = clean_stack( newops, 4 );
    return newops;
}

static inline u8_t *newops_pushds(u8_t *newops)
{
    newops = push_reg( newops, OP_REG_EAX ); // Create stack space.
    newops = op_call( newops, (u8_t **)burn_read_ds );
    return newops;
}

static inline u8_t *newops_popds(u8_t *newops)
{
    newops = op_call( newops, (u8_t **)burn_write_ds );
    newops = clean_stack( newops, 4 );
    return newops;
}

static inline u8_t *newops_pushes(u8_t *newops)
{
    newops = push_reg( newops, OP_REG_EAX ); // Create stack space.
    newops = op_call( newops, (u8_t **)burn_read_es );
    return newops;
}

static inline u8_t *newops_popes(u8_t *newops)
{
    newops = op_call( newops, (u8_t **)burn_write_es );
    newops = clean_stack( newops, 4 );
    return newops;
}

static inline u8_t *newops_movfromseg(u8_t *newops, ia32_modrm_t modrm, u8_t *suffixes)
{
    if( !modrm.is_register_mode() )
	newops = push_reg( newops, OP_REG_EAX ); // Preserve EAX
    newops = push_reg( newops, OP_REG_EAX ); // Create stack space.
    switch( modrm.get_reg() ) {
	case 0:
	    newops = op_call( newops, (u8_t **)burn_read_es );
	    break;
	case 1:
	    newops = op_call( newops, (u8_t **)burn_read_cs );
	    break;
	case 2:
	    newops = op_call( newops, (u8_t **)burn_read_ss );
	    break;
	case 3:
	    newops = op_call( newops, (u8_t **)burn_read_ds );
	    break;
	case 4:
	    newops = op_call( newops, (u8_t **)burn_read_fs );
	    break;
	case 5:
	    newops = op_call( newops, (u8_t **)burn_read_gs );
	    break;
	default:
	    con << "unknown segment @ " << (u8_t **)newops << '\n';
	    DEBUGGER_ENTER();
	    break;
    }
    if( modrm.is_register_mode() )
	newops = pop_reg( newops, modrm.get_rm() );
    else {
	// We can't pop the modrm, because it is a 16-bit operand.
	newops = pop_reg( newops, OP_REG_EAX );	// Get result.
	newops = insert_operand_size_prefix( newops );	// 16-bit
	newops = mov_to_modrm( newops, OP_REG_EAX, modrm, suffixes );
	newops = pop_reg( newops, OP_REG_EAX ); // Restore EAX.
    }
    return newops;
}
static inline u8_t *newops_cli(u8_t *newops)
{
    newops = op_call(newops, (void*) burn_cli);
    return newops;
}

static u8_t *newops_pushfs(u8_t *newops)
{
    newops = push_reg( newops, OP_REG_EAX );
    newops = op_call( newops, (u8_t **)burn_read_fs );
    return newops;
}

static u8_t *newops_popfs(u8_t *newops)
{
    newops = op_call( newops, (u8_t **)burn_write_fs );
    newops = clean_stack( newops, 4 );
    return newops;
}

static u8_t *newops_pushgs(u8_t *newops)
{	
    newops = push_reg( newops, OP_REG_EAX );
    newops = op_call( newops, (u8_t **)burn_read_gs );
    return newops;
}

static u8_t *newops_popgs(u8_t *newops)
{	
    newops = op_call( newops, (u8_t **)burn_write_gs );
    newops = clean_stack( newops, 4 );
    return newops;
}

static u8_t *newops_movfromcreg(u8_t *newops, ia32_modrm_t modrm, u8_t *suffixes)
{	
    // The modrm reg field is the control register.
    // The modrm r/m field is the general purpose register.
    ASSERT( modrm.is_register_mode() );
    ASSERT( (modrm.get_reg() != 1) && 
	    (modrm.get_reg() <= 4) );
    newops = push_reg( newops, OP_REG_EAX ); // Allocate
    newops = push_byte( newops, modrm.get_reg() );
    newops = op_call( newops, (u8_t **)burn_read_cr );
    newops = clean_stack( newops, 4 );
    newops = pop_reg( newops, modrm.get_rm() );
    return newops;
}

#endif /* !__L4KA__REWRITE_STACKLESS_H__ */

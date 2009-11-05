/*********************************************************************
 *                
 * Copyright (C) 2009 Jan Stoess
 *
 * File path:	glue/earm_cpu.h
 * Description:	Declarations for EARM cpu mgmt.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: server.h,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/
#include <l4/types.h>
#define L4VM_LOGID_OFFSET               (L4_LOG_ROOTSERVER_LOGID + 1)
extern L4_Word64_t L4VM_earm_get_cpu_energy(void);
extern void L4VM_earm_cpu_init(void);

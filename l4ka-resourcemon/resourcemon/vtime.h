/*********************************************************************
 *                
 * Copyright (C) 2006,  Karlsruhe University
 *                
 * File path:     vtime.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __RESOURCEMON__VTIME_H__
#define __RESOURCEMON__VTIME_H__

#include <l4/thread.h>
bool associate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t handler_tid, L4_Word_t cpu);
bool deassociate_virtual_timer_interrupt(vm_t *vm, const L4_ThreadId_t caller_tid, L4_Word_t cpu);

#endif /* !__HOME__STOESS__RESOURCEMON__RESOURCEMON__VTIME_H__ */

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
bool associate_virtual_timer_interrupt(const L4_ThreadId_t handler_tid);
bool deassociate_virtual_timer_interrupt(const L4_ThreadId_t caller_tid);

#endif /* !__HOME__STOESS__RESOURCEMON__RESOURCEMON__VTIME_H__ */

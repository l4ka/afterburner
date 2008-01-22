/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburner-wedge/include/l4ka/l4privileged.h
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
 * $Id: l4privileged.h,v 1.6 2005/08/26 15:42:34 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__L4PRIVILEGED_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__L4PRIVILEGED_H__

#include <string.h>
#include <hiostream.h>
#include <l4/kip.h>

INLINE bool l4_has_feature( char *feature_name )
{
    void *kip = L4_GetKernelInterface();
    char *name;

    for( L4_Word_t i = 0; (name = L4_Feature(kip,i)) != '\0'; i++ )
	if( !strcmp(feature_name, name) )
	    return true;
    return false;
}


typedef L4_Word_t L4_Error_t;

extern const char * L4_ErrString( L4_Error_t err );

extern L4_Error_t ThreadControl( 
	L4_ThreadId_t dest, L4_ThreadId_t space,
	L4_ThreadId_t sched, L4_ThreadId_t pager, L4_Word_t utcb, L4_Word_t prio = 0 );

extern L4_Error_t SpaceControl( L4_ThreadId_t dest, L4_Word_t control, 
	L4_Fpage_t kip, L4_Fpage_t utcb, L4_ThreadId_t redir );

extern L4_Error_t DeassociateInterrupt( L4_ThreadId_t irq_tid );
extern L4_Error_t AssociateInterrupt( 
	L4_ThreadId_t irq_tid, L4_ThreadId_t handler_tid );



#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__L4PRIVILEGED_H__ */

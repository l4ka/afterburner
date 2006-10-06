/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/resourcemon.h
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
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__RESOURCEMON_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__RESOURCEMON_H__

#include INC_ARCH(types.h)

#include "resourcemon_idl_client.h"

extern IResourcemon_shared_t resourcemon_shared;

extern inline bool contains_device_mem(L4_Word_t low, L4_Word_t high)
{
    if (low == high)
	return false;
    
    for( L4_Word_t d=0; d<IResourcemon_max_devices;d++ )
    {
	if (resourcemon_shared.devices[d].low ==
		resourcemon_shared.devices[d].high)
	    return false;
	
	if ((resourcemon_shared.devices[d].low >= low && 
	     resourcemon_shared.devices[d].low < high) ||
	    (resourcemon_shared.devices[d].high > low &&
	     resourcemon_shared.devices[d].high <= high) ||
	    (resourcemon_shared.devices[d].low <= low &&
	     resourcemon_shared.devices[d].high >= high))
	    return true;
    }
    return false;
	    
}

extern bool l4ka_server_locate( guid_t guid, L4_ThreadId_t *server_tid );
extern bool cmdline_key_search( const char *key, char *value, word_t n );

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__RESOURCEMON_H__ */

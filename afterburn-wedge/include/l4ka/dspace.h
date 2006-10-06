/*********************************************************************
 *
 * Copyright (C) 2006,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/dspace.h
 * Description:   Data space support.
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__DSPACE_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__DSPACE_H__

#include <l4/types.h>

typedef int (*dspace_pfault_handler_t)(word_t fault_addr, word_t *ip, L4_Fpage_t *map_fp, void *data );

class dspace_handlers_t
{
    const static word_t count = 3;

    dspace_pfault_handler_t handlers[count];
    void *data[count];

    word_t total;

public:
    dspace_handlers_t()
    {
	total = 0;
	for( word_t i = 0; i < count; i++ ) {
	    handlers[i] = NULL;
	    data[i] = NULL;
	}
    }

    void add_dspace_handler( dspace_pfault_handler_t handler, void *new_data )
    {
	ASSERT( total < count );
	handlers[total] = handler;
	data[total] = new_data;
	total++;
    }

    bool handle_pfault( word_t fault_addr, word_t *ip, L4_Fpage_t *map_fp )
    {
	for( word_t i = 0; i < total; i++ )
	    if( handlers[i](fault_addr, ip, map_fp, data[i]) )
		return true;
	return false;
    }
};

extern dspace_handlers_t dspace_handlers;

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__DSPACE_H__ */

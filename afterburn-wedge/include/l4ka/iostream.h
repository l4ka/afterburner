/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/iostream.h
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
 * $Id: iostream.h,v 1.3 2005/04/13 15:47:33 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__

#include INC_WEDGE(sync.h)
#include <l4/kdebug.h>
#include <hiostream.h>

class hiostream_kdebug_t : public hiostream_driver_t
{
    cpu_lock_t lock;

public:
    virtual void init()
	{ 	    
	    this->color = this->background = unknown; 
	    lock.init(); 
	}
	    
    virtual void print_char( char ch )
	{ 
	    if (!lock.is_locked_by_tid(L4_Myself()))
		lock.lock();
	    
	    L4_KDB_PrintChar( ch ); 
	    __asm__ __volatile__("mfence\n\t");
	     
	    if(ch == '\n' && lock.is_locked_by_tid(L4_Myself()))
	    	lock.unlock();
	}

    virtual char get_blocking_char()
	{ return L4_KDB_ReadChar_Blocked(); }
};


#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__ */

/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:	afterburn-wedge/common/gcc_support.cc
 * Description:	gcc generates code which relies on runtime support 
 * 		libraries.  Unfortunately, gcc's runtime support libraries
 * 		also have dependencies on glibc.  So this file helps
 * 		avoid linking against those runtime support libraries.
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
 * $Id: gcc_support.cc,v 1.2 2005/04/13 11:09:53 joshua Exp $
 *
 ********************************************************************/

extern "C" void
__cxa_pure_virtual( void )
{
    // This is a place-holder function used by gcc while building objects
    // with virtual functions.  It should never be called, unless someone 
    // invokes an object method before gcc has finished constructing the
    // object.  Make sure that this doesn't happen!
}


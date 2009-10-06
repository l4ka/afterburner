/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/ia32/page.h
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
 * $Id: page.h,v 1.3 2005/04/13 15:47:31 joshua Exp $
 *
 ********************************************************************/
#ifndef __IA32__PAGE_H__
#define __IA32__PAGE_H__

#define PAGEDIR_BITS	22
#define PAGEDIR_SIZE	(__UL(1) << PAGEDIR_BITS)
#define PAGEDIR_MASK	(~(PAGEDIR_SIZE-1))

#define SUPERPAGE_BITS	22
#define SUPERPAGE_SIZE	(__UL(1) << SUPERPAGE_BITS)
#define SUPERPAGE_MASK	(~(SUPERPAGE_SIZE - 1))

#define PAGE_BITS	12
#define PAGE_SIZE	(__UL(1) << PAGE_BITS)
#define PAGE_MASK	(~(PAGE_SIZE-1))


#endif	/* __IA32__PAGE_H__ */

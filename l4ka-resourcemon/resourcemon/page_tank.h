/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/page_tank.h
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
#ifndef __HYPERVISOR__INCLUDE__PAGE_TANK_H__
#define __HYPERVISOR__INCLUDE__PAGE_TANK_H__

class page_tank_page_t
{
public:
    page_tank_page_t *next;

    L4_Word_t get_page()
	{ return L4_Word_t(this) & PAGE_MASK; }
};

class page_tank_t
{
private:
    page_tank_page_t *free_page_list;

public:
    page_tank_t()
	{ this->free_page_list = NULL; }

    L4_Word_t alloc_page()
	{
	    if( NULL == this->free_page_list )
		return NULL;

	    L4_Word_t page = this->free_page_list->get_page();
	    this->free_page_list = this->free_page_list->next;
	    return page;
	}

    bool free_page( L4_Word_t page )
	{
	    bool was_empty = (NULL == this->free_page_list);

	    page = page & PAGE_MASK;
	    ((page_tank_page_t *)page)->next = this->free_page_list;
	    this->free_page_list = (page_tank_page_t *)page;

	    return was_empty;
	}
};

extern inline page_tank_t * get_page_tank()
{
    extern page_tank_t page_tank;
    return &page_tank;
}

#endif	/* __HYPERVISOR__INCLUDE__PAGE_TANK_H__ */

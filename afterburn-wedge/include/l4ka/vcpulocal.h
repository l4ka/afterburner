/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/vcpulocal.h
 * Description:   Thread-local declarations.
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
 * $Id: vcpulocal.h,v 1.6 2005/04/13 15:47:33 joshua Exp $
 *
 ********************************************************************/
#ifndef __L4KA__VMCOMMON__VCPULOCAL_H__
#define __L4KA__VMCOMMON__VCPULOCAL_H__
#include <l4/thread.h>

#include INC_ARCH(cpu.h)
#include INC_WEDGE(debug.h)
#define OFS_VCPU_CPUID	   32


extern word_t	 start_vcpulocal, end_vcpulocal, sizeof_vcpulocal, start_vcpulocal_shadow, end_vcpulocal_shadow;
#define VCPULOCAL(subsection)		__attribute__((section(".vcpulocal." subsection)))
#define IS_VCPULOCAL(addr)		((word_t) addr >= start_vcpulocal && (word_t) addr < (end_vcpulocal))

template<typename T> INLINE T *get_on_vcpu(T *ptr, const word_t vcpu_id)
{
    return ((T *) ((word_t) ptr - start_vcpulocal + start_vcpulocal_shadow + (sizeof_vcpulocal * vcpu_id)));
}
   

template<typename T> INLINE T &get_vcpulocal(T &t, word_t vcpu_id = CONFIG_NR_VCPUS)
{
    
    if (vcpu_id == CONFIG_NR_VCPUS )
    {
#if defined(CONFIG_SMP_ONE_AS)
	vcpu_id = * (word_t *) (L4_UserDefinedHandle() + OFS_VCPU_CPUID);
	ASSERT(vcpu_id < CONFIG_NR_VCPUS);
	return *get_on_vcpu(&t,  vcpu_id);
#else
	return t;
#endif
    }
    else 
    {
	T &t_on_vcpu = *get_on_vcpu(&t, vcpu_id);
	return t_on_vcpu;
    }
}


#endif /* !__L4KA__VMCOMMON__VCPULOCAL_H__ */

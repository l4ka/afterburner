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
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__TLOCAL_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__TLOCAL_H__

#include <l4/thread.h>

#include INC_ARCH(cpu.h)
#include INC_WEDGE(vcpu.h)


extern word_t	 start_vcpulocal, end_vcpulocal, sizeof_vcpulocal, start_vcpulocal_shadow, end_vcpulocal_shadow;
#define VCPULOCAL(subsection)		__attribute__((section(".vcpulocal." subsection)))
#define IS_VCPULOCAL(addr)		((word_t) addr >= start_vcpulocal && (word_t) addr < (end_vcpulocal))
#define GET_ON_VCPU(v, type, ptr)	\
    ((type *) ((word_t) ptr - start_vcpulocal + start_vcpulocal_shadow + (sizeof_vcpulocal * v)))


INLINE vcpu_t & get_vcpu(const word_t vcpu_id) __attribute__((const));
INLINE vcpu_t & get_vcpu(const word_t vcpu_id)
{
    ASSERT(vcpu_id < CONFIG_NR_VCPUS);
    extern vcpu_t vcpu;
    return  *GET_ON_VCPU(vcpu_id, vcpu_t, &vcpu);  
    
}

INLINE vcpu_t & get_vcpu() __attribute__((const));
INLINE vcpu_t & get_vcpu()
{
    extern vcpu_t vcpu;
    ASSERT(vcpu.is_valid_vcpu());
    return vcpu;
}

INLINE cpu_t & get_cpu() __attribute__((const));
INLINE cpu_t & get_cpu()
    // Get the thread local architecture CPU object.  Return a reference, so 
    // that by definition, we must return a valid object.
{
    return get_vcpu().cpu;
}

INLINE void set_vcpu( vcpu_t &vcpu )
{
    ASSERT(vcpu.is_valid_vcpu());
    ASSERT(vcpu.cpu_id < CONFIG_NR_VCPUS);
    //L4_Set_UserDefinedHandle( (L4_Word_t)vcpu );
}


INLINE local_apic_t & get_lapic(const word_t vcpu_id) __attribute__((const));
INLINE local_apic_t & get_lapic(const word_t vcpu_id)
{
    ASSERT(vcpu_id < CONFIG_NR_VCPUS);
    extern local_apic_t lapic;
    local_apic_t &lapic_on_vcpu = *GET_ON_VCPU(vcpu_id, local_apic_t, &lapic);
    return lapic_on_vcpu;
}


INLINE local_apic_t & get_lapic() __attribute__((const));
INLINE local_apic_t & get_lapic()
{
    extern local_apic_t lapic;
    return lapic;
}



#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__TLOCAL_H__ */

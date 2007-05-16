/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/eacc.h
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

#ifndef __EACC_H__
#define __EACC_H__

#include <common/ia32/page.h>

#define EARM_ACC_DEBUG
#undef  EARM_ACC_DEBUG_EVAL
#define EARM_ACC_DEBUG_MSEC                    1000
#define EARM_ACC_DEBUG_CPU
#define EARM_ACC_DEBUG_DISK
#define EARM_ACC_DEBUG_MIN_DOMAIN                  0
#define EARM_ACC_DEBUG_MIN_RESOURCE		   0


#define EARM_ACC_MIN_DOMAIN                        0
#define EARM_ACCCPU_MSEC			  20
#define EARM_EASCPU_MSEC			 100
#define EARM_EASDISK_MSEC			 200

#define THROTTLE_DISK    // Disk throttling
#define THROTTLE_CPU    // CPU throttling

#define MIN_CPU_DOMAIN             3
#define DELTA_CPU_POWER         1000
#define INIT_CPU_POWER        100000

#define MIN_DISK_DOMAIN           4
#define DELTA_DISK_POWER         50
#define INIT_DISK_POWER        100000
#define INIT_DISK_THROTTLE     (6000)

#define DTF	4 // Disk throttle factor

#define EARM_ACC_UNLIMITED (~0ULL)

static const bool debug_earmdisk = 0;
extern L4_Word64_t debug_pmc[8];

/* *********** PMCs */

#include <common/ia32/msr.h>

// Some additional MSRs not found in msr.h
#define MSR_IA32_PEBS_ENABLE                    0x3f1
#define MSR_IA32_PEBS_MATRIX_VERT               0x3f2

#define CONFIG_X_EARM_PENTIUM_D
#undef CONFIG_X_EARM_PENTIUM_4

#if defined(CONFIG_X_EARM_PENTIUM_4)
#define EARM_PMC_TSC_WEIGHT		    (617)	    
#define EARM_PMC_UC_WEIGHT		    (712)
#define EARM_PMC_MQW_WEIGHT		    (475)
#define EARM_PMC_RB_WEIGHT		     (56)
#define EARM_PMC_MB_WEIGHT		  (34046)
#define EARM_PMC_MR_WEIGHT		    (173)		  
#define EARM_PMC_MLR_WEIGHT		   (2996)
#define EARM_PMC_LDM_WEIGHT		   (1355)
#elif defined(CONFIG_X_EARM_PENTIUM_D)

/* 
 * jsXXX: recalibrated TSC and UC:
 *  - ran only hypervisor, without VMs
 *  - made the throttler thread either execute "hlt" or real cycles
 *  - obtained the performance counter events, they differed in TSC/UC mostly
 *  - obtained real CPU power via labview, both 
 *  - results (10 sec runtime): 
 *    HLT   : 29999 tsc +   190 uc  ( + 3 rb + 13 mr ) = 43309
 *    NOHLT : 29981 tsc + 29981 uc  ( + 4 rb + 13 mr ) = 85934
 */
#define EARM_PMC_TSC_WEIGHT		   (1435) //recalibrated, old was (1270)   	  
#define EARM_PMC_UC_WEIGHT		   (1432) //recalibrated, old was (1660)
#define EARM_PMC_MQW_WEIGHT		   (0)
#define EARM_PMC_RB_WEIGHT		   (1007) 
#define EARM_PMC_MB_WEIGHT		   (0)
#define EARM_PMC_MR_WEIGHT		   (598)    	 
#define EARM_PMC_MLR_WEIGHT		   (0)
#define EARM_PMC_LDM_WEIGHT		   (19200)
#else
#error please define EARM PMC weights
#endif

#define NR_WEIGHTS 5
static const L4_Word64_t pmc_weight[NR_WEIGHTS] = {EARM_PMC_UC_WEIGHT, EARM_PMC_RB_WEIGHT, 
						EARM_PMC_MR_WEIGHT,  EARM_PMC_LDM_WEIGHT,
						EARM_PMC_TSC_WEIGHT};


class eacc_t
{
private:
    static const word_t nr_pcpus = 2;//IResourcemon_max_cpus;
    static const word_t nr_pm_counters = 18;
    static const word_t fpage_size = ((PAGE_SIZE - 1 + 2 * nr_pcpus * NR_WEIGHTS * sizeof(L4_Word64_t))/PAGE_SIZE)*PAGE_SIZE;
    word_t pmc_mask[nr_pm_counters];
    L4_Word8_t pmc_mapping[2 * fpage_size - 1];
    L4_Word64_t *pmc_diff_energy[nr_pcpus];
    L4_Word64_t *pmc_raw_energy[nr_pcpus];
    L4_Word64_t pmc_raw[nr_pcpus][NR_WEIGHTS];
    void write_msr(const word_t addr, const L4_Word64_t buf);
    void print();
    L4_Word_t get_pmc_mapping_start_addr(void)
	{
	    return reinterpret_cast<L4_Word_t>(pmc_diff_energy[0]);
	}

    L4_Word_t get_sizeof_fpage(void)
	{
	    return ((PAGE_SIZE - 1 + 2 * nr_pcpus * NR_WEIGHTS * sizeof(L4_Word64_t))/PAGE_SIZE)*PAGE_SIZE;
	}
public:
    void init(void);
    void pmc_setup(void);
    void write(word_t pcpu);
    void map(idl4_fpage_t *fp);

};

extern eacc_t eacc;

#endif /* !__EACC_H__ */

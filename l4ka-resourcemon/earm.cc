/*********************************************************************
 *                
 * Copyright (C) 2005-2007, 2009,  Karlsruhe University
 *                
 * File path:     earm.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "earm.h"

void pmc_setup()
{
        L4_Word64_t buf;

        // Configure ESCRs

        buf = ((L4_Word64_t)0x0 << 32) | 0xFC00;
        x86_wrmsr(MSR_IA32_TC_PRECISE_EVENT, buf);

        // Enable Precise Event Based Sampling (accurate & low sampling overhead)
        buf = ((L4_Word64_t)0x0 << 32) | 0x1000001;
        x86_wrmsr(MSR_IA32_PEBS_ENABLE, buf);

        // Also enabling PEBS
        buf = ((L4_Word64_t)0x0 << 32) | 0x1;
        x86_wrmsr(MSR_IA32_PEBS_MATRIX_VERT, buf);

        // Count unhalted cycles
        buf = ((L4_Word64_t)0x0 << 32) | 0x2600020C;
        x86_wrmsr(MSR_IA32_FSB_ESCR0, buf);

        // Count load uops that are replayed due to unaligned addresses
        // and/or partial data in the Memory Order Buffer (MOB)
        buf = ((L4_Word64_t)0x0 << 32) | 0x600740C;
        x86_wrmsr(MSR_IA32_MOB_ESCR0, buf);

        // Count op queue writes
        buf = ((L4_Word64_t)0x0 << 32) | 0x12000E0C;
	x86_wrmsr(MSR_IA32_MS_ESCR0, buf);

        // Count retired branches
        buf = ((L4_Word64_t)0x0 << 32) | 0x8003C0C;
        x86_wrmsr(MSR_IA32_TBPU_ESCR0, buf);

        // Unknown
        buf = ((L4_Word64_t)0x0 << 32) | 0x900000C;
	x86_wrmsr(MSR_IA32_FIRM_ESCR0, buf);

        // Count mispredicted
        buf = ((L4_Word64_t)0x0 << 32) | 0x600020C;
	x86_wrmsr(MSR_IA32_CRU_ESCR0, buf);

        // Count memory retired
        buf = ((L4_Word64_t)0x0 << 32) | 0x1000020C;
	x86_wrmsr(MSR_IA32_CRU_ESCR2, buf);

        // Count load miss level 1 data cache
        buf = ((L4_Word64_t)0x0 << 32) | 0x1200020C;
	x86_wrmsr(MSR_IA32_CRU_ESCR3, buf);

        // Unknown
        buf = ((L4_Word64_t)0x0 << 32) | 0x4000C0C;
	x86_wrmsr(MSR_IA32_RAT_ESCR0, buf);

        // Configure CCCRs

        // Store unhalted cycles
        buf = ((L4_Word64_t)0x0 << 32) | 0x3D000;
	x86_wrmsr(MSR_IA32_BPU_CCCR0, buf);

        // Store MOB load replay
        buf = ((L4_Word64_t)0x0 << 32) | 0x35000;
	x86_wrmsr(MSR_IA32_BPU_CCCR1, buf);

        // Storee op queue writes
        buf = ((L4_Word64_t)0x0 << 32) | 0x31000;
	x86_wrmsr(MSR_IA32_MS_CCCR0, buf);

        // Store retired branches
        buf = ((L4_Word64_t)0x0 << 32) | 0x35000;
	x86_wrmsr(MSR_IA32_MS_CCCR1, buf);

        // Store something unknown
        buf = ((L4_Word64_t)0x0 << 32) | 0x33000;
	x86_wrmsr(MSR_IA32_FLAME_CCCR0, buf);

        // Store mispredicted branches
        buf = ((L4_Word64_t)0x0 << 32) | 0x39000;
	x86_wrmsr(MSR_IA32_IQ_CCCR0, buf);

        // Store memory retired
        buf = ((L4_Word64_t)0x0 << 32) | 0x3B000;
	x86_wrmsr(MSR_IA32_IQ_CCCR1, buf);

        // Store load miss level 1 data cache
        buf = ((L4_Word64_t)0x0 << 32) | 0x3b000;
	x86_wrmsr(MSR_IA32_IQ_CCCR2, buf);

        // Store something unknown
        buf = ((L4_Word64_t)0x0 << 32) | 0x35000;
	x86_wrmsr(MSR_IA32_IQ_CCCR4, buf);

        // Setup complete

}


void earm_init() 
{

    printf("Init energy management \n");

    pmc_setup();
    /* Start resource manager */
    earmmanager_init();
    /* Start CPU resource */
    earmcpu_init();
    
    /* Start ea scheduler */
    earmeas_init();
    
}

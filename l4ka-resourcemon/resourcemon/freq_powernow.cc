/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     freq_powernow.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <resourcemon/freq_powernow.h>
#include <common/debug.h>
#include <resourcemon/resourcemon.h>

#define rdmsr(msr,val1,val2) \
	__asm__ __volatile__("rdmsr" \
			: "=a" (val1), "=d" (val2) \
			: "c" (msr))
#define wrmsr(msr,val1,val2) \
	__asm__ __volatile__("wrmsr" \
			: /* no outputs */ \
			: "c" (msr), "a" (val1), "d" (val2))

static struct powernow_data_t powernow_data;

static u32_t currpstate[IResourcemon_max_cpus];	// keep track of the current pstate of each cpu


u32_t powernow_get_freq(void) {
	// MANUEL: system call - optimize by handing over the pcpu number
	u32_t state = currpstate[L4_ProcessorNo()];
	printf("DVFS: current pstate is %d with freq %d\n ", 
	       state, powernow_data.pstates[state].core_freq);
	return powernow_data.pstates[state].core_freq;
}

u32_t powernow_get_vdd(void) {
	u32_t hi = 0, lo = 0;
	rdmsr(MSR_COFVID_STATUS, lo, hi);
	u32_t cur_nbvid = (lo & CUR_NBVID_MASK) >> CUR_NBVID_SHIFT;
	u32_t cur_cpuvid = (lo & CUR_CPUVID_MASK) >> CUR_CPUVID_SHIFT;
	u32_t max_vid = (hi & MAX_VID_MASK) >> MAX_VID_SHIFT;
	u32_t min_vid = (hi & MIN_VID_MASK) >> MIN_VID_SHIFT;
	printf("DVFS: NB VID: %d CPU VID: %d MIN VID: %d MAX VID: %d\n",
	       cur_nbvid, cur_cpuvid, min_vid, max_vid);
	return cur_cpuvid; // TODO: Calculate voltage
}

int powernow_target(unsigned target_freq/*, unsigned relation*/) {
	L4_Word_t pcpu = L4_ProcessorNo();
	unsigned int newstate = 0;
	u32_t hi = 0, lo = 0;

	/* only run on specific CPU from here on */
	// MANUEL: the virq threads are always bound to their respective cpu

	//hout << "powernow_target: cpu " << pcpu 
	//	<< ", " << target_freq << " MHz\n";

	/* MANUEL: find target pstate (with minimal frequency >= targfreq
	 * cpufreq_frequency_table_target(pol, data->powernow_table, targfreq, relation, &newstate)
	 */
	for (unsigned int i = 0; i < powernow_data.pstate_count; i++) {
		if (powernow_data.pstates[i].core_freq < target_freq) {
			if (powernow_data.pstates[i].core_freq > powernow_data.pstates[newstate].core_freq) {
				// should not happen if we start with newstate = 0
				L4_KDB_Enter("powernowk8_target: newstate_freq < target_freq"); 
				newstate = i;
			}
		} else {
			if (powernow_data.pstates[i].core_freq < powernow_data.pstates[newstate].core_freq)
				newstate = i;
		}
	}
	newstate = powernow_data.pstates[newstate].control & HW_PSTATE_MASK;
	if (newstate > powernow_data.max_hw_pstate) {
		L4_KDB_Enter("invalid pstate!");
		return 1;
	}
	rdmsr(MSR_PSTATE_DEF_BASE + newstate, lo, hi);
	if (!(hi & HW_PSTATE_VALID_MASK)) {
		L4_KDB_Enter("invalid pstate!");
		return 1;
	}

	printf("DVFS: cpu %d transition to index %d with freq %d\n",
	       pcpu, newstate, powernow_data.pstates[newstate].core_freq);
	wrmsr(MSR_PSTATE_CTRL, newstate, 0);
	currpstate[pcpu] = newstate;

	rdmsr(MSR_PSTATE_STATUS, lo, hi);
	if ((lo & HW_PSTATE_MASK) != newstate) {
		L4_KDB_Enter("pstate transition failed!");
		return 1;
	}

	return 0;
}

#define ACPI_D_WORD_PREFIX	0x0c // bytecode identifying an object as 32 bit word
#define ACPI_PACKAGE_OP		0x12 // bytecode identifying an object as package
#define ACPI_PSS_LENGTH		32	 // length of a PSS object
#define ACPI_PSS_COUNT		6	 // # entries in a PSS object

static void powernow_extract_pss(u8_t* pss) {
	u8_t* cur = pss; // Our "package" stream begins with the NameString
	u32_t length = 0;
	u32_t pss_count = 0;

	cur += 4; // skip to PackageOp
	if (*cur++ != ACPI_PACKAGE_OP) L4_KDB_Enter("not a package!");
	u8_t msbs = *cur >> 6; // first byte of PkgLength
	// hout << "first byte: " << *cur << '\n';
	// hout << "MSBs: " << msbs << '\n';
	if (msbs == 0) {
		length = *cur++ & 0x3f;
	} else {
		length = *cur++ & 0x0f;
		for (int i = 1; i <= msbs; i++) {
			length += *cur++ << (4 + 8*(i-1));
		}
	}
	// hout << "length: " << length << '\n';
	pss_count = *cur++;
	powernow_data.pstate_count = pss_count;
	if (pss_count > MAX_PSTATES - 1) L4_KDB_Enter("Too many Pstates!");
	for (u32_t i = 0; i < pss_count; i++) { // extract the actual PSS objects
		if (*cur++ != ACPI_PACKAGE_OP) L4_KDB_Enter("not a package!");
		if (*cur++ != ACPI_PSS_LENGTH) L4_KDB_Enter("not a PSS object! (wrong length)");
		if (*cur++ != ACPI_PSS_COUNT) L4_KDB_Enter("not a PSS object! (wrong count)");
		for (int j = 0; j < 6; j++) {
			if (((acpi_dword_t*)cur)->prefix != ACPI_D_WORD_PREFIX) L4_KDB_Enter("not a DWord!");
			((u32_t*)powernow_data.pstates)[i*6 + j] = ((acpi_dword_t*)cur)->value;
			cur += 5;
		}
	}
	powernow_data.pstates[pss_count].core_freq = 0;
	powernow_data.min_freq = ~0;
	powernow_data.max_freq = 0;
	for (u32_t i = 0; i < pss_count; i++) {
		if (powernow_data.pstates[i].core_freq < powernow_data.min_freq)
			powernow_data.min_freq = powernow_data.pstates[i].core_freq;
		if (powernow_data.pstates[i].core_freq > powernow_data.max_freq)
			powernow_data.max_freq = powernow_data.pstates[i].core_freq;
	}
	/*
	for (u32_t i = 0; i < powernow_data.pstate_count; i++){
		hout << "Pstate " << i << ":\n";
		hout << " CoreFreq: " << powernow_data.pstates[i].core_freq << '\n';
		hout << " Power: " << powernow_data.pstates[i].power << '\n';
		hout << " TransitionLacenty: " << powernow_data.pstates[i].transition_latency << '\n';
		hout << " BusMasterLatency: " << powernow_data.pstates[i].bus_master_latency << '\n';
		hout << " Control: " << powernow_data.pstates[i].control << '\n';
		hout << " Status: " << powernow_data.pstates[i].status << "\n\n";
	}
	*/
}

void powernow_init(L4_Word_t num_cpus) {
    acpi_rsdp_t* rsdp = acpi_rsdp_t::locate(ACPI20_PC99_RSDP_START);
    printf("ACPI DVFS: RSDP is at %x\n", rsdp);
	
    acpi_rsdt_t *rsdt = rsdp->rsdt();
    acpi_xsdt_t *xsdt = rsdp->xsdt();
    printf("ACPI DVFS: RSDT is at %x\n", rsdt);
    printf("ACPI DVFS: XSDT is at %x\n", xsdt);
	
    // acpi_fadt_t *fadt;
    acpi_dsdt_t *ssdt;
    if (xsdt != NULL) {
	// xsdt->list();
	// fadt = (acpi_fadt_t*)xsdt->find("FACP");
	ssdt = (acpi_dsdt_t*)xsdt->find("SSDT");
    } else {
	// rsdt->list();
	// fadt = (acpi_fadt_t*)rsdt->find("FACP");
	ssdt = (acpi_dsdt_t*)rsdt->find("SSDT");
    }
    //((acpi_thead_t*)fadt)->show();
    ((acpi_thead_t*)ssdt)->show();
	
    //acpi_dsdt_t *dsdt = (acpi_dsdt_t*)fadt->get_dsdt();
    //((acpi_thead_t*)dsdt)->show();
	
    u8_t* pss = ssdt->get_package("_PSS", 1);
    powernow_extract_pss(pss);

    printf("ACPI DVFS: number of pstates: %d\n",  powernow_data.pstate_count);
    for (u32_t i = 0; i < powernow_data.pstate_count; i++)
	printf("\tpstate %d has freq %d\n", i, powernow_data.pstates[i].core_freq);

    u32_t hi = 0, lo = 0;
    rdmsr(MSR_PSTATE_CUR_LIMIT, hi, lo);
    powernow_data.max_hw_pstate = (hi & HW_PSTATE_MAX_MASK) >> HW_PSTATE_MAX_SHIFT;

    for (u32_t i = 0; i < num_cpus; i++){
	currpstate[i] = 0;
    }
}

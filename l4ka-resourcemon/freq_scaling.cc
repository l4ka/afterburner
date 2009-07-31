/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     freq_scaling.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <freq_powernow.h>
#include <freq_scaling.h>
#include <debug.h>

void freq_adjust_pstate(void ) {
	u32_t curfreq = powernow_get_freq();
	u32_t newfreq = 0;
	switch (curfreq) {
		case 1900:
			newfreq = 1700; // exact
			break;
		case 1700:
			newfreq = 1190; // something below
			break;
		case 1400:
			newfreq = 27; // far below
			break;
		case 1200:
			newfreq = 1201; // only just above lower
			break;
		case 1000:
			newfreq = 2700; // above highest
			break;
		default:
		    printf("curfreq: %d\n", curfreq);
		    L4_KDB_Enter("unexpected current frequency");
	}
	newfreq = 400; // to test voltage scaling
	if (powernow_target(newfreq))
		L4_KDB_Enter("powernow_target failed");
	powernow_get_vdd();
	/*
	hout << "freq_adjust_pstate: old frequency was " << curfreq 
		<< ", requested frequency " << newfreq 
		<< ", got " << powernow_get() << '\n';
	*/
}

void freq_migrate(void) {
}

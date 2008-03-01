/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     device/rtc.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __DEVICE__RTC_H__
#define __DEVICE__RTC_H__

#include <aftertime.h>
#include INC_ARCH(intlogic.h)

static const u32_t mc146818_irq = 8;

class rtc_t {
    time_t last_update;

public:
    u8_t seconds, minutes, hours;
    u8_t week_day, day, month, year;
    
    u32_t periodic_freq;
    bool periodic_enabled;
    u64_t last_tick_us;
    
    void enable_periodic()
	{ periodic_enabled = true; }

    void disable_periodic()
	{ periodic_enabled = false; }

    void set_periodic_frequency(u32_t freq)
	{ periodic_freq = freq; }

    u32_t get_periodic_frequency()
	{ return periodic_freq; }

    void periodic_tick(u64_t now_us)
	{
#if !defined(CONFIG_DEVICE_PASSTHRU) || defined(CONFIG_WEDGE_L4KA)
	    if (!periodic_enabled) 
		return;
	    
	    u64_t delta = (now_us - last_tick_us);
	    last_tick_us = now_us;
	    
	    if (delta > (1000 * 1000 / periodic_freq))
		get_intlogic().raise_irq(mc146818_irq);
#endif
	}
    
    void do_update();

    time_t get_last_update()
	{ return last_update; }


    rtc_t()
	{ last_update = 0; }
};

extern rtc_t rtc;

#endif /* !__DEVICE__RTC_H__ */

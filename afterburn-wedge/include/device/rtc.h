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
    
    u8_t system_flags;
    
    void enable_periodic()
	{ periodic_enabled = true; }

    void disable_periodic()
	{ periodic_enabled = false; }

    void set_periodic_frequency(u32_t freq)
	{ 
	    printf("RTC periodic freq %d\n", freq);
	    periodic_freq = freq; 
	} 

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
	    {
		dprintf(debug_rtc, "rtc periodic timer\n");
		get_intlogic().raise_irq(mc146818_irq);
	    }
#endif
	}
    
    void do_update();

    time_t get_last_update()
	{ return last_update; }
    
    u8_t get_system_flags()
	{ return system_flags; }

    void set_system_flags(word_t flags)
	{ 
	    //	2Dh 45 1 byte System Operational Flags 
	    //       Bit 7 = Weitek processor present/absent 
	    //       Bit 6 = Floppy drive seek at boot enable/disable 
	    //       Bit 5 = System boot sequence (1:hd, 0:fd)
	    //       Bit 4 = System boot CPU speed high/low 
	    //       Bit 3 = External cache enable/disable 
	    //       Bit 2 = Internal cache enable/disable 
	    //       Bit 1 = Fast gate A20 operation enable/disable 
	    //       Bit 0 = Turbo switch function enable/disable 
	    
	    printf("CMOS ignored system flags write (gate A20) %x\n", flags);
	    system_flags = flags;
		
	}


    rtc_t()
	{ last_update = 0; system_flags = 0x20; }
};

extern rtc_t rtc;

INLINE void legacy_0x92( u16_t port, u32_t &value, u32_t bit_width, bool read )
{
#if defined(CONFIG_DEVICE_PASSTHRU) && !defined(CONFIG_WEDGE_L4KA)
	UNIMPLEMENTED();
#else
    if( read ) 
	value = rtc.get_system_flags();
    else
	rtc.set_system_flags(value);
#endif
}

#endif /* !__DEVICE__RTC_H__ */

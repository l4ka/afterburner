/*
 * QEMU 8253/8254 interval timer emulation
 * 
 * Copyright (c) 2003-2004 Fabrice Bellard
 * Copyright (c) 2006 Intel Corperation
 * Copyright (c) 2007 Keir Fraser, XenSource Inc.
 * Copyright (c) 2009 University of Karlsruhe
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <device/portio.h>
#include INC_WEDGE(vcpulocal.h)
#include <console.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(intmath.h)
#include INC_ARCH(cycles.h)
#include INC_ARCH(intlogic.h)

#define RW_STATE_LSB 1
#define RW_STATE_MSB 2
#define RW_STATE_WORD0 3
#define RW_STATE_WORD1 4

#define PIT_FREQ 1193181

#define TIMER_DISABLED 0
#define TIMER_PERIODIC 1
#define TIMER_ONESHOT  2

#define DIV_ROUND(x, y) (((x) + (y) / 2) / (y))

typedef struct PITChannel {
        u32_t count; /* can be 65536 */
        u16_t latched_count;
        u8_t count_latched;
        u8_t status_latched;
        u8_t status;
        u8_t read_state;
        u8_t write_state;
        u8_t write_latch;
        u8_t rw_mode;
        u8_t mode;
        u8_t bcd; /* not supported */
        u8_t gate; /* timer start */
} PITChannel;

typedef struct PITState {
    /* Channel state */
    PITChannel channels[3];
    /* Last time the counters read zero, for calcuating counter reads */
    s64_t count_load_time[3];
    u32_t speaker_data_on;
    u32_t pad0;
    /* Channel 0 IRQ handling. */
    u32_t ch0_period; // in usecs
    u8_t  ch0_timer_mode; // disabled, periodic, one-shot
} PITState;


static PITState pit_state;

static int pit_get_count(PITState *pit, int channel)
{
    u64_t d;
    int  counter;
    PITChannel *c = &pit->channels[channel];

    /*    d = muldiv64(get_cycles() - pit->count_load_time[channel],
	  PIT_FREQ, ticks_per_sec);*/
    d = muldiv64(L4_SystemClock().raw - pit->count_load_time[channel],
		 PIT_FREQ, 1000000UL);

    switch ( c->mode )
    {
    case 0:
    case 1:
    case 4:
    case 5:
        counter = (c->count - d) & 0xffff;
        break;
    case 3:
        /* XXX: may be incorrect for odd counts */
        counter = c->count - ((2 * d) % c->count);
        break;
    default:
        counter = c->count - (d % c->count);
        break;
    }
    return counter;
}

static int pit_get_out(PITState *pit, int channel)
{
    PITChannel *s = &pit->channels[channel];
    u64_t d;
    int out;

    /*d = muldiv64(get_cycles() - pit->count_load_time[channel], 
      PIT_FREQ, ticks_per_sec);*/
    d = muldiv64(L4_SystemClock().raw - pit->count_load_time[channel],
		 PIT_FREQ, 1000000UL);

    switch ( s->mode )
    {
    default:
    case 0:
        out = (d >= s->count);
        break;
    case 1:
        out = (d < s->count);
        break;
    case 2:
        out = (((d % s->count) == 0) && (d != 0));
        break;
    case 3:
        out = ((d % s->count) < ((s->count + 1) >> 1));
        break;
    case 4:
    case 5:
        out = (d == s->count);
        break;
    }

    return out;
}

static void pit_set_gate(PITState *pit, int channel, int val)
{
    PITChannel *s = &pit->channels[channel];

    switch ( s->mode )
    {
    default:
    case 0:
    case 4:
        /* XXX: just disable/enable counting */
        break;
    case 1:
    case 5:
    case 2:
    case 3:
        /* Restart counting on rising edge. */
        if ( s->gate < val )
            pit->count_load_time[channel] = L4_SystemClock().raw;
        break;
    }

    s->gate = val;
}

int pit_get_gate(PITState *pit, int channel)
{
    return pit->channels[channel].gate;
}

static inline void pit_time_fired(u64_t *priv)
{
    // unused
    //u64_t *count_load_time = (u64_t*)priv;
    //*priv = get_cycles();
}

static void pit_load_count(PITState *pit, int channel, int val)
{
    PITChannel *s = &pit->channels[channel];

    if ( val == 0 )
        val = 0x10000;


    pit->count_load_time[channel] = L4_SystemClock().raw;
    s->count = val;

    if (channel != 0)
        return;

    pit->ch0_period = DIV_ROUND((val * 1000000ULL), PIT_FREQ);

    printf("pit load count: val %d\n",val);
    printf("pit period: %u\n", pit->ch0_period);

    switch ( s->mode )
    {
        case 2:
            /* Periodic timer. */
            //create_periodic_time(v, &pit->pt0, period, 0, 0, pit_time_fired, 
            //                     &pit->count_load_time[channel]);
	    printf("create periodic timer\n");
	    pit->ch0_timer_mode = TIMER_PERIODIC;
            break;
        case 1:
            /* One-shot timer. */
            //create_periodic_time(v, &pit->pt0, period, 0, 1, pit_time_fired,
            //                     &pit->count_load_time[channel]);
	    printf("create oneshot timer\n");
	    pit->ch0_timer_mode = TIMER_ONESHOT;
            break;
        default:
            //destroy_periodic_time(&pit->pt0);
	    printf("disable timer\n");
	    pit->ch0_timer_mode = TIMER_DISABLED;
            break;
    }
}

static void pit_latch_count(PITState *pit, int channel)
{
    PITChannel *c = &pit->channels[channel];

    if ( !c->count_latched )
    {
        c->latched_count = pit_get_count(pit, channel);
        c->count_latched = c->rw_mode;
    }
}

static void pit_latch_status(PITState *pit, int channel)
{
    PITChannel *c = &pit->channels[channel];

    if ( !c->status_latched )
    {
        /* TODO: Return NULL COUNT (bit 6). */
        c->status = ((pit_get_out(pit, channel) << 7) |
                     (c->rw_mode << 4) |
                     (c->mode << 1) |
                     c->bcd);
        c->status_latched = 1;
    }
}

static void pit_ioport_write(struct PITState *pit, u32_t addr, u32_t val)
{
    int channel, access;
    PITChannel *s;

    val  &= 0xff;
    addr &= 3;

    if ( addr == 3 )
    {
        channel = val >> 6;
        if ( channel == 3 )
        {
            /* Read-Back Command. */
            for ( channel = 0; channel < 3; channel++ )
            {
                s = &pit->channels[channel];
                if ( val & (2 << channel) )
                {
                    if ( !(val & 0x20) )
                        pit_latch_count(pit, channel);
                    if ( !(val & 0x10) )
                        pit_latch_status(pit, channel);
                }
            }
        }
        else
        {
            /* Select Counter <channel>. */
            s = &pit->channels[channel];
            access = (val >> 4) & 3;
            if ( access == 0 )
            {
                pit_latch_count(pit, channel);
            }
            else
            {
                s->rw_mode = access;
                s->read_state = access;
                s->write_state = access;
                s->mode = (val >> 1) & 7;
                if ( s->mode > 5 )
                    s->mode -= 4;
                s->bcd = val & 1;
                /* XXX: update irq timer ? */
            }
        }
    }
    else
    {
        /* Write Count. */
        s = &pit->channels[addr];
        switch ( s->write_state )
        {
        default:
        case RW_STATE_LSB:
            pit_load_count(pit, addr, val);
            break;
        case RW_STATE_MSB:
            pit_load_count(pit, addr, val << 8);
            break;
        case RW_STATE_WORD0:
            s->write_latch = val;
            s->write_state = RW_STATE_WORD1;
            break;
        case RW_STATE_WORD1:
            pit_load_count(pit, addr, s->write_latch | (val << 8));
            s->write_state = RW_STATE_WORD0;
            break;
        }
    }

}

static u32_t pit_ioport_read(struct PITState *pit, u32_t addr)
{
    int ret, count;
    PITChannel *s;
    
    addr &= 3;
    s = &pit->channels[addr];

    if ( s->status_latched )
    {
        s->status_latched = 0;
        ret = s->status;
    }
    else if ( s->count_latched )
    {
        switch ( s->count_latched )
        {
        default:
        case RW_STATE_LSB:
            ret = s->latched_count & 0xff;
            s->count_latched = 0;
            break;
        case RW_STATE_MSB:
            ret = s->latched_count >> 8;
            s->count_latched = 0;
            break;
        case RW_STATE_WORD0:
            ret = s->latched_count & 0xff;
            s->count_latched = RW_STATE_MSB;
            break;
        }
    }
    else
    {
        switch ( s->read_state )
        {
        default:
        case RW_STATE_LSB:
            count = pit_get_count(pit, addr);
            ret = count & 0xff;
            break;
        case RW_STATE_MSB:
            count = pit_get_count(pit, addr);
            ret = (count >> 8) & 0xff;
            break;
        case RW_STATE_WORD0:
            count = pit_get_count(pit, addr);
            ret = count & 0xff;
            s->read_state = RW_STATE_WORD1;
            break;
        case RW_STATE_WORD1:
            count = pit_get_count(pit, addr);
            ret = (count >> 8) & 0xff;
            s->read_state = RW_STATE_WORD0;
            break;
        }
    }

    return ret;
}

void pit_stop_channel0_irq(PITState *pit)
{
// unused
//    spin_lock(&pit->lock);
//    destroy_periodic_time(&pit->pt0);
//    spin_unlock(&pit->lock);
}


void pit_init()
{
    PITState *pit = &pit_state;
    PITChannel *s;
    int i;

    printf("pit init\n");
    
    for ( i = 0; i < 3; i++ )
    {
        s = &pit->channels[i];
        s->mode = 0xff; /* the init mode */
        s->gate = (i != 2);
        pit_load_count(pit, i, 0);
    }

}

u32_t pit_get_remaining_usecs()
{
    struct PITState *pit = &pit_state;

    if(pit->ch0_timer_mode == TIMER_DISABLED)
	return 54925;

    u32_t d = L4_SystemClock().raw - pit->count_load_time[0];

    ASSERT(d >= 0);

    if( d >= pit->ch0_period)
	return 0;
    else
	return (pit->ch0_period - d);
}

void pit_handle_timer_interrupt()
{
    struct PITState *pit = &pit_state;
    
    if(pit->ch0_timer_mode == TIMER_DISABLED)
	return;

    u32_t d = L4_SystemClock().raw - pit->count_load_time[0];

    ASSERT(d >= 0);
    if( d >= pit->ch0_period)
    {
	/* time skew correction, max one period */
	u32_t c = min( d - pit->ch0_period, pit->ch0_period);
	pit->count_load_time[0] = L4_SystemClock().raw - c;

	if(pit->ch0_timer_mode == TIMER_ONESHOT)
	    pit->ch0_timer_mode = TIMER_DISABLED;

	get_intlogic().raise_irq(INTLOGIC_TIMER_IRQ);
    }

}

/* the intercept action for PIT DM retval:0--not handled; 1--handled */  
static int handle_pit_io(
    u32_t port, u32_t &val, u32_t bit_width, bool read)
{
    struct PITState *pit = &pit_state;

    if ( bit_width != 8 )
    {
        printf("PIT bad access\n");
        return 1;
    }

    if ( !read )
    {
        pit_ioport_write(pit, port, val);
    }
    else
    {
        if ( (port & 3) != 3 )
            val = pit_ioport_read(pit, port);
        else
            printf("PIT: read A1:A0=3!\n");
    }

    return 1;
}

static void speaker_ioport_write(
    struct PITState *pit, u32_t addr, u32_t val)
{
    pit->speaker_data_on = (val >> 1) & 1;
    pit_set_gate(pit, 2, val & 1);
}

static u32_t speaker_ioport_read(
    struct PITState *pit, u32_t addr)
{
    /* Refresh clock toggles at about 15us. We approximate as 2^4us. */
    unsigned int refresh_clock = ((unsigned int)L4_SystemClock().raw >> 4) & 1;
    return ((pit->speaker_data_on << 1) | pit_get_gate(pit, 2) |
            (pit_get_out(pit, 2) << 5) | (refresh_clock << 4));
}

static int handle_speaker_io(
    u32_t port, u32_t &val, u32_t bit_width, bool read)
{
    struct PITState *pit = &pit_state;

    printf("speaker io\n");

    if ( bit_width != 8 )
    {
        printf("PIT_SPEAKER bad access\n");
        return 1;
    }

    if ( read )
        val = speaker_ioport_read(pit, port);        
    else
	speaker_ioport_write(pit, port, val);

    return 1;
}


void i8254_portio( u16_t port, u32_t & value, u32_t bit_width, bool read )
{
    //dbg_printf("i8254 io\n");
    if ( port == 0x61)
	handle_speaker_io(port, value, bit_width, read);
    else
	handle_pit_io(port, value, bit_width, read);
}


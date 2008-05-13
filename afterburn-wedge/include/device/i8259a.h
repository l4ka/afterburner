/*
 * QEMU 8259 interrupt controller emulation
 * 
 * Copyright (c) 2003-2004 Fabrice Bellard
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __AFTERBURN_WEDGE__INCLUDE__DEVICE__I8259A_H__
#define __AFTERBURN_WEDGE__INCLUDE__DEVICE__I8259A_H__

#include INC_ARCH(bitops.h)
#include INC_ARCH(cpu.h)
#include INC_WEDGE(vcpu.h)
#include <debug.h>

#define debug_i8259a_irq	\
    ((irq_dbg_level(irq).level < debug_i8259a.level) ? irq_dbg_level(irq) : debug_i8259a)

class i8259a_t;

class i8259a_pic_t 
{
public:
    u8_t last_irr; /* edge detection */
    u8_t irr; /* interrupt request register */
    u8_t last_imr; /* hardware interrupt unmask/enable detection */
    u8_t imr; /* interrupt mask register */
    u8_t isr; /* interrupt service register */
    u8_t priority_add; /* highest irq priority */
    u8_t irq_base;
    u8_t read_reg_select;
    u8_t poll;
    u8_t special_mask;
    u8_t init_state;
    u8_t auto_eoi;
    u8_t rotate_on_auto_eoi;
    u8_t special_fully_nested_mode;
    u8_t init4; /* true if 4 byte init */
    u8_t elcr; /* PIIX edge/trigger selection*/
    u8_t elcr_mask;
    i8259a_t *pics_state;
    
    
    bool is_master(); 
    bool is_level_triggered(u8_t irq) 
	{ return (elcr & (1 << (irq))); }
	
    void reset()
	{

	    last_irr = 0;
	    irr = 0;
	    imr = 0;
	    last_imr = 0;
	    isr = 0;
	    priority_add = 0;
	    irq_base = 0;
	    read_reg_select = 0;
	    poll = 0;
	    special_mask = 0;
	    init_state = 0;
	    auto_eoi = 0;
	    rotate_on_auto_eoi = 0;
	    special_fully_nested_mode = 0;
	    init4 = 0;
	    /* Note: ELCR is not reset */
	}

    /* return the highest priority found in mask (highest = smallest
       number). Return 8 if no irq */
    int get_priority(int mask)
	{
	    int priority;
	    if (mask == 0)
		return 8;
	    priority = 0;
	    while ((mask & (1 << ((priority + priority_add) & 7))) == 0)
		priority++;
	    return priority;
	}
    
    /* return the pic wanted interrupt. return -1 if none */
    int get_irq()
	{
	    int mask, cur_priority, priority;

	    mask = irr & ~imr;
	    priority = get_priority(mask);
    
	    if (priority == 8)
		return -1;
	    /* compute current priority. If special fully nested mode on the
	       master, the IRQ coming from the slave is not taken into account
	       for the priority computation. */
	    mask = isr;
	    if (special_fully_nested_mode && is_master())
		mask &= ~(1 << 2);
	    cur_priority = get_priority(mask);
	    if (priority < cur_priority) {
		/* higher priority found: an irq should be generated */
		return (priority + priority_add) & 7;
	    } else {
		return -1;
	    }
    
	}

    /* set irq level. If an edge is detected, then the IRR is set to 1 */
    void set_irq(int irq, int level);
    /* return the pic wanted interrupt. return -1 if none */
    void ack_irq(int irq);

    void unmask_hwirq(int irq);
    void unmask_hwirqs();
    
    u32_t poll_read (u32_t addr1);
    
    void ioport_write(u32_t addr, u32_t val);
    u32_t ioport_read(u32_t addr1);
    
    void dump(void)
	{
	    dprintf(debug_i8259a, "i8259a %c: irr=%02x imr=%02x isr=%02x hprio=%d irq_base=%02x rr_sel=%d elcr=%02x fnm=%d\n",
		    (is_master() ? '0' : '1'),
		    irr, imr, isr, priority_add, 
		    irq_base, read_reg_select, elcr, 
		    special_fully_nested_mode);

	}
    


};

//(irq ? debug_i8259a: debug_id_t(0,3))
class i8259a_t
{
public:
    /* 0 is master pic, 1 is slave pic */
    i8259a_pic_t pics[2];
    
    
    /* raise irq to CPU if necessary. must be called every time the active
       irq may change */
    void update_irq()
	{
	    int irq2, irq;

	    /* first look at slave pic */
	    irq2 = pics[1].get_irq();
	    if (irq2 >= 0) 
	    {
		/* if irq request by slave pic, signal master PIC */
		pics[0].set_irq(2, 1);
		pics[0].set_irq(2, 0);
	    }
	    /* look at requested irq */
	    irq = pics[0].get_irq();
	    if (irq >= 0) 
	    {
		for(int i = 0; i < 2; i++) 
		{
		    dprintf(debug_i8259a_irq, "i8259a %d: update irq set pending\n", i, irq); 
		    pics[i].dump();
		}
		    
		/* Set master and slave irq cluster bits */
		get_cpu().set_irq_vector(0);
		get_cpu().set_irq_vector(8);

	    }
	    else {
		for(int i = 0; i < 2; i++) 
		{
		    dprintf(debug_i8259a_irq, "i8259a %d: update irq clear pending\n", i);
		    pics[i].dump();
		}
 
		/* Clear master and slave irqs */
		get_cpu().clear_irq_vector(0);
		get_cpu().clear_irq_vector(8);
	    }
	}
   
public:    
    
    bool pending_vector( word_t & vector, word_t & irq)
	{
	    word_t irq2;

	    irq = pics[0].get_irq();
	    
	    if (irq >= 0) 
	    {
		pics[0].ack_irq(irq);
		
		if (irq == 2) 
		{
		    irq2 = pics[1].get_irq();
		    if (irq2 >= 0) 
			pics[1].ack_irq(irq2);
		    else 
			/* spurious IRQ on slave controller */
			irq2 = 7;
		    
		    vector = pics[1].irq_base + irq2;
		    irq = irq2 + 8;
		} 
		else 
		{
		    vector = pics[0].irq_base + irq;
		}
	    } 
	    else 
	    {
		/* spurious IRQ on host controller */
		irq = 7;
		vector = pics[0].irq_base + irq;
	    }	    
	    update_irq();
	    
	    dprintf(debug_i8259a_irq, "i8259a: pending irq %d vector %d %c\n", 
		    irq, vector, (get_cpu().interrupts_enabled() ? 'I' : 'i'));
	    return true;
	}
	
    void raise_irq(word_t irq)
	{ 
	    dprintf(debug_i8259a_irq, "i8259a: raise irq %d\n", irq);
	    word_t p = irq >> 3;
	    irq &= 7;
	    pics[p].set_irq(irq, 1);
	    if (!pics[p].is_level_triggered(irq))
		pics[p].set_irq(irq, 0);
	    update_irq();
	}
    
    void reraise_vector(word_t vector)
	{
	    for(int p = 0; p < 2; p++) 
		if (vector >= pics[p].irq_base && vector <= (pics[p].irq_base + 8U))
		{
		    word_t irq = vector - pics[p].irq_base;
		    
		    dprintf(debug_i8259a_irq, "i8259a: reraise vector=%d irq %d\n", vector, irq);
		    pics[p].isr &= ~(1 << irq);
		    if (p == 1) 
			pics[0].isr &= ~(1 << 2);
		    pics[p].set_irq(irq, 1);
		    
		    if (!pics[p].is_level_triggered(irq))
			pics[p].set_irq(irq, 0);
		    
		    update_irq();
		    return;
		}
	}


    
   
    i8259a_t()
	{
	    pics[0].reset();
	    pics[1].reset();
	    pics[0].elcr_mask = 0xf8;
	    pics[1].elcr_mask = 0xde;
	    pics[0].pics_state = this;
	    pics[1].pics_state = this;
	    
	}

};

#endif	/* __AFTERBURN_WEDGE__INCLUDE__DEVICE__I8259A_H__ */

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


#include INC_ARCH(types.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(backend.h)

i8259a_t i8259a;

bool i8259a_pic_t::is_master() 
{ 
    return (this == &pics_state->pics[0]); 
}

/* set irq level. If an edge is detected, then the IRR is set to 1 */
void i8259a_pic_t::set_irq(int irq, int level)
{
    int mask;
    mask = 1 << irq;
    if (elcr & mask) 
    {
	DEBUGGER_ENTER("Level triggered IRQs untested\n");
	dbg_irq(irq + (is_master() ? 0 : 8));
	/* level triggered */
	if (level) {
	    irr |= mask;
	    last_irr |= mask;
	} else {
	    irr &= ~mask;
	    last_irr &= ~mask;
	}
    } else {
	/* edge triggered */
	if (level) {
	    if ((last_irr & mask) == 0)
		irr |= mask;
	    last_irr |= mask;
	} else {
	    last_irr &= ~mask;
	}
    }
    dprintf(debug_i8259a_irq, "i8259a %c: set irq %d level %d\n",
	    (is_master() ? '0' : '1'), irq, level);
}

void i8259a_pic_t::unmask_hwirq(int irq)
{
    word_t hwirq = irq + (is_master() ? 0 : 8);
    
    if (!get_intlogic().is_hwirq_squashed(hwirq) &&
	get_intlogic().test_and_clear_hwirq_mask(hwirq))
    {
	dprintf(debug_i8259a_irq, "i8259a: unmask hwirq %d hwirq %d\n", irq, hwirq);
	bool saved_int_state = get_cpu().disable_interrupts();
	backend_unmask_device_interrupt(hwirq);
	get_cpu().restore_interrupts(saved_int_state);
    }
}

/* return the pic wanted interrupt. return -1 if none */
void i8259a_pic_t::ack_irq(int irq)
{
   
    if (auto_eoi) {
	if (rotate_on_auto_eoi)
	    priority_add = (irq + 1) & 7;
    } else {
	isr |= (1 << irq);
    }
    
    /* We don't clear level sensitive interrupt here */
    if (!(elcr & (1 << irq)))
    {
	dprintf(debug_i8259a_irq, "i8259a: ack edge-triggered irq %d\n", irq);
	irr &= ~(1 << irq);
    }
    else
	dprintf(debug_i8259a_irq, "i8259a: ack level-triggered irq %d\n", irq);
    

}

void i8259a_pic_t::unmask_hwirqs()
{
    intlogic_t &intlogic = get_intlogic();
    u8_t hwirq_base = is_master() ? 0 : 8;
    
    /* pending but masked ? */
    if( (irr & last_imr ) & ~imr)
	get_cpu().set_irq_vector(hwirq_base);

    word_t last_imr32 = last_imr;
    word_t imr32 = imr;
    
    word_t last_hwirq_mask = ((last_imr32 << irq_base) | ~( 0xff << irq_base));  
    word_t hwirq_mask = ((imr32 << irq_base) | ~( 0xff << irq_base)); 
    
    // Those unmasked & those not latched & those not squashed
    word_t newly_enabled = ~hwirq_mask 
	& ~intlogic.get_hwirq_latch() 
	& ~intlogic.get_hwirq_squash();

    while( newly_enabled ) 
    {
	word_t irq = lsb( newly_enabled );
	bit_clear( irq, newly_enabled );
	    
	intlogic.set_hwirq_latch(irq);
	    
	dprintf(debug_i8259a_irq, "i8259a %c enable irq %d, deve %x latch %x\n", 
	       (is_master() ? '0' : '1'), irq, newly_enabled, intlogic.get_hwirq_latch());
	if( !backend_enable_device_interrupt(irq, get_vcpu()) )
	    printf( "Unable to enable passthru device interrupt %d\n", irq);
		    
    }


    // Those masked before & those unmasked now & those masked in hw
    // & those not squashed

    word_t device_unmasked = last_hwirq_mask & ~hwirq_mask
	& intlogic.get_hwirq_mask() 
	& ~intlogic.get_hwirq_squash();
	
    while( device_unmasked ) 
    {
	word_t irq = lsb( device_unmasked );
	bit_clear( irq, device_unmasked );
	intlogic.clear_hwirq_mask(irq);

	dprintf(debug_i8259a_irq, "i8259a %c unmask irq %d, mask %x\n", 
		(is_master() ? '0' : '1'), irq, intlogic.get_hwirq_mask());
	backend_unmask_device_interrupt( irq );
    }
}
void i8259a_pic_t::ioport_write(u32_t addr, u32_t val)
{
    int priority, cmd, irq;

    dprintf(debug_i8259a, "i8259a write: addr=0x%02x val=0x%02x\n", addr, val);
    
    if (addr >= 0x4d0)
    { 
	elcr = val & elcr_mask; 
	return;
    }  
	
    addr &= 1;
    if (addr == 0) 
    {
        if (val & 0x10) {
            /* init */
            reset();
            /* deassert a pending interrupt */
	    get_cpu().clear_irq_vector(irq_base);
            init_state = 1;
            init4 = val & 1;
            if (val & 0x02)
                printf("i8259a: single mode not supported");
            if (val & 0x08)
                printf("i8259a: level sensitive irq not supported");
        } else if (val & 0x08) {
            if (val & 0x04)
                poll = 1;
            if (val & 0x02)
                read_reg_select = val & 1;
            if (val & 0x40)
                special_mask = (val >> 5) & 1;
        } else {
            cmd = val >> 5;
            switch(cmd) {
            case 0:
            case 4:
                rotate_on_auto_eoi = cmd >> 2;
                break;
            case 1: /* end of interrupt */
            case 5:
                priority = get_priority(isr);
                if (priority != 8) {
                    irq = (priority + priority_add) & 7;
                    isr &= ~(1 << irq);
		    dprintf(debug_i8259a_irq, "eoi irq %d\n", irq);
		    unmask_hwirq(irq);
                    if (cmd == 5)
                        priority_add = (irq + 1) & 7;
                    pics_state->update_irq();
                }
                break;
            case 3:
                irq = val & 7;
                isr &= ~(1 << irq);
		unmask_hwirq(irq);
		dprintf(debug_i8259a_irq, "eoi irq %d\n", irq);
                pics_state->update_irq();
                break;
            case 6:
                priority_add = (val + 1) & 7;
                pics_state->update_irq();
                break;
            case 7:
                irq = val & 7;
                isr &= ~(1 << irq);
		dprintf(debug_i8259a_irq, "eoi irq %d\n", irq);
		unmask_hwirq(irq);
                priority_add = (irq + 1) & 7;
                pics_state->update_irq();
                break;
            default:
                /* no operation */
                break;
            }
        }
    } else 
    {
	switch(init_state) 
	{
        case 0:
            /* normal mode */
	    last_imr = imr;
	    imr = val;
            unmask_hwirqs();
	    pics_state->update_irq();
            break;
        case 1:
            irq_base = val & 0xf8;
            init_state = 2;
            break;
        case 2:
            if (init4) {
                init_state = 3;
            } else {
                init_state = 0;
            }
            break;
        case 3:
            special_fully_nested_mode = (val >> 4) & 1;
            auto_eoi = (val >> 1) & 1;
            init_state = 0;
            break;
        }
    }
}

u32_t i8259a_pic_t::poll_read (u32_t addr1)
{
    int ret;

    ret = get_irq();
    
    if (ret >= 0) {
        if (addr1 >> 7) {
            pics_state->pics[0].isr &= ~(1 << 2);
            pics_state->pics[0].irr &= ~(1 << 2);
	    unmask_hwirq(2);
        }
        irr &= ~(1 << ret);
        isr &= ~(1 << ret);
	unmask_hwirq(ret);
        if (addr1 >> 7 || ret != 2)
	    pics_state->update_irq();
    } else {
        ret = 0x07;
	pics_state->update_irq();
    }

    return ret;
}

u32_t i8259a_pic_t::ioport_read(const u32_t addr1)
{
    unsigned int addr;
    int ret;

    if (addr1 >= 0x4d0)
    { 
	return elcr;
    }  

    addr = addr1;
    addr &= 1;
    if (poll) {
        ret = poll_read(addr1);
        poll = 0;
    } else {
        if (addr == 0) {
            if (read_reg_select)
                ret = isr;
            else
                ret = irr;
        } else {
            ret = imr;
        }
    }
    dprintf(debug_i8259a, "i8259a read: addr=0x%02x val=0x%02x\n", addr1, ret);
    return ret;
}




void i8259a_portio( u16_t port, u32_t & value, bool read )
{
    i8259a_pic_t *pic = NULL;
    
    switch (port)
    {
    case 0x20:
    case 0x21:
    case 0x4d1:
	pic = &i8259a.pics[0];
	break;
    case 0xa0:
    case 0xa1:
    case 0x4d0:
	pic = &i8259a.pics[1];
	break;
    }
    ASSERT(pic);
	
    if (read)
	value = pic->ioport_read(port);
    else
	pic->ioport_write(port, value);
}

INLINE word_t
port_byte_result( word_t eax, u8_t result )
{
    return (eax & ~word_t(0xff)) | result;
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0x20_in( word_t eax )
{
    INC_BURN_COUNTER(8259_master_read);
    return port_byte_result(eax, i8259a.pics[0].ioport_read(0x20));
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0x21_in( word_t eax )
{
    INC_BURN_COUNTER(8259_master_read);
    return port_byte_result(eax, i8259a.pics[0].ioport_read(0x21));
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0xa0_in( word_t eax )
{
    INC_BURN_COUNTER(8259_slave_read);
    return port_byte_result(eax, i8259a.pics[1].ioport_read(0xa0));
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0xa1_in( word_t eax )
{
    INC_BURN_COUNTER(8259_slave_read);
    return port_byte_result(eax, i8259a.pics[1].ioport_read(0xa1));
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0x20_out( word_t eax )
{
    INC_BURN_COUNTER(8259_master_write);
    i8259a.pics[0].ioport_write(0x20, eax);
    return eax;
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0x21_out( word_t eax )
{
    INC_BURN_COUNTER(8259_master_write);
    i8259a.pics[0].ioport_write(0x21, eax);
    return eax;
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0xa0_out( word_t eax )
{
    INC_BURN_COUNTER(8259_slave_write);
    i8259a.pics[1].ioport_write(0xa0, eax);
    return eax;
}

extern "C" __attribute__((regparm(1)))
word_t device_8259_0xa1_out( word_t eax )
{
    INC_BURN_COUNTER(8259_slave_write);
    i8259a.pics[1].ioport_write(0xa1, eax);
    return eax;
}

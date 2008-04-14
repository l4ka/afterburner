/*
 * QEMU PC keyboard emulation
 * 
 * Copyright (c) 2003 Fabrice Bellard
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

/* debug PC keyboard */
/* debug PC keyboard : only mouse */
//#define DEBUG_MOUSE

/*	Keyboard Controller Commands */
#define I8042_CCMD_READ_MODE	0x20	/* Read mode bits */
#define I8042_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define I8042_CCMD_GET_VERSION	0xA1	/* Get controller version */
#define I8042_CCMD_MOUSE_DISABLE	0xA7	/* Disable mouse interface */
#define I8042_CCMD_MOUSE_ENABLE	0xA8	/* Enable mouse interface */
#define I8042_CCMD_TEST_MOUSE	0xA9	/* Mouse interface test */
#define I8042_CCMD_SELF_TEST	0xAA	/* Controller self test */
#define I8042_CCMD_I8042_TEST	0xAB	/* Keyboard interface test */
#define I8042_CCMD_I8042_DISABLE	0xAD	/* Keyboard interface disable */
#define I8042_CCMD_I8042_ENABLE	0xAE	/* Keyboard interface enable */
#define I8042_CCMD_READ_INPORT    0xC0    /* read input port */
#define I8042_CCMD_READ_OUTPORT	0xD0    /* read output port */
#define I8042_CCMD_WRITE_OUTPORT	0xD1    /* write output port */
#define I8042_CCMD_WRITE_OBUF	0xD2
#define I8042_CCMD_WRITE_AUX_OBUF	0xD3    /* Write to output buffer as if
					   initiated by the auxiliary device */
#define I8042_CCMD_WRITE_MOUSE	0xD4	/* Write the following byte to the mouse */
#define I8042_CCMD_DISABLE_A20    0xDD    /* HP vectra only ? */
#define I8042_CCMD_ENABLE_A20     0xDF    /* HP vectra only ? */
#define I8042_CCMD_RESET	        0xFE

/* Keyboard Commands */
#define I8042_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define I8042_CMD_ECHO     	0xEE
#define I8042_CMD_GET_ID 	        0xF2	/* get keyboard ID */
#define I8042_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define I8042_CMD_ENABLE		0xF4	/* Enable scanning */
#define I8042_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define I8042_CMD_RESET_ENABLE   	0xF6    /* reset and enable scanning */
#define I8042_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define I8042_REPLY_POR		0xAA	/* Power on reset */
#define I8042_REPLY_ACK		0xFA	/* Command ACK */
#define I8042_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/* Status Register Bits */
#define I8042_STAT_OBF 		0x01	/* Keyboard output buffer full */
#define I8042_STAT_IBF 		0x02	/* Keyboard input buffer full */
#define I8042_STAT_SELFTEST	0x04	/* Self test successful */
#define I8042_STAT_CMD		0x08	/* Last write was a command write (0=data) */
#define I8042_STAT_UNLOCKED	0x10	/* Zero if keyboard locked */
#define I8042_STAT_MOUSE_OBF	0x20	/* Mouse output buffer full */
#define I8042_STAT_GTO 		0x40	/* General receive/xmit timeout */
#define I8042_STAT_PERR 		0x80	/* Parity error */

/* Controller Mode Register Bits */
#define I8042_MODE_I8042_INT	0x01	/* Keyboard data generate IRQ1 */
#define I8042_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ12 */
#define I8042_MODE_SYS 		0x04	/* The system flag (?) */
#define I8042_MODE_NO_KEYLOCK	0x08	/* The keylock doesn't affect the keyboard if set */
#define I8042_MODE_DISABLE_I8042	0x10	/* Disable keyboard interface */
#define I8042_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */
#define I8042_MODE_KCC 		0x40	/* Scan code conversion to PC format */
#define I8042_MODE_RFU		0x80

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL		0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define I8042_QUEUE_SIZE 256

#define I8042_PENDING_I8042         1
#define I8042_PENDING_AUX         2

#include INC_ARCH(types.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(backend.h)
#include <device/i8042.h>

class i8042_t {
    
private:
    void reset();
    void update_irq();
    void queue(int b, int aux);
    
public:
    static void update_i8042_irq(int level);
    static void update_i8042_aux_irq(int level);

    i8042_t()
	{
	    reset();
	    ps2_kbd_init(update_i8042_irq);
	    ps2_mouse_init(update_i8042_aux_irq);
	}
    
    u32_t read();
    void write(u32_t val);
    u32_t read_status();
    void write_command(u32_t val);


    u8_t write_cmd; /* if non zero, write data to port 60 is expected */
    u8_t status;
    u8_t mode;
    /* Bitmask of devices with data available.  */
    u8_t pending;
};

/* update irq and I8042_STAT_[MOUSE_]OBF */
/* XXX: not generating the irqs if I8042_MODE_DISABLE_I8042 is set may be
   incorrect, but it avoids having to simulate exact delays */
void i8042_t::update_irq()
{
    int irq12_level, irq1_level;

    irq1_level = 0;    
    irq12_level = 0;    
    status &= ~(I8042_STAT_OBF | I8042_STAT_MOUSE_OBF);
    if (pending) {
        status |= I8042_STAT_OBF;
        /* kdb data takes priority over aux data.  */
        if (pending == I8042_PENDING_AUX) {
            status |= I8042_STAT_MOUSE_OBF;
            if (mode & I8042_MODE_MOUSE_INT)
                irq12_level = 1;
        } else {
            if ((mode & I8042_MODE_I8042_INT) && 
                !(mode & I8042_MODE_DISABLE_I8042))
                irq1_level = 1;
        }
    }
    
    //dbg_irq(1); dbg_irq(12);
    
    get_intlogic().raise_irq(1);
    get_intlogic().raise_irq(12);
}

u32_t i8042_t::read_status()
{
    if (status != 0x18)
	dprintf(debug_i8042, "i8042 read status %x\n", status);
    return status;
}

void i8042_t::queue(int b, int aux)
{
    if (aux)
        ps2_queue_mouse(b);
    else
        ps2_queue_kbd(b);
}

void i8042_t::write_command(u32_t val)
{
    dprintf(debug_i8042, "i8042 write command %x\n", val);

    switch(val) {
    case I8042_CCMD_READ_MODE:
        queue(mode, 0);
        break;
    case I8042_CCMD_WRITE_MODE:
    case I8042_CCMD_WRITE_OBUF:
    case I8042_CCMD_WRITE_AUX_OBUF:
    case I8042_CCMD_WRITE_MOUSE:
    case I8042_CCMD_WRITE_OUTPORT:
        write_cmd = val;
        break;
    case I8042_CCMD_MOUSE_DISABLE:
        mode |= I8042_MODE_DISABLE_MOUSE;
        break;
    case I8042_CCMD_MOUSE_ENABLE:
        mode &= ~I8042_MODE_DISABLE_MOUSE;
        break;
    case I8042_CCMD_TEST_MOUSE:
        queue(0x00, 0);
        break;
    case I8042_CCMD_SELF_TEST:
        status |= I8042_STAT_SELFTEST;
        queue(0x55, 0);
        break;
    case I8042_CCMD_I8042_TEST:
        queue(0x00, 0);
        break;
    case I8042_CCMD_I8042_DISABLE:
        mode |= I8042_MODE_DISABLE_I8042;
        update_irq();
        break;
    case I8042_CCMD_I8042_ENABLE:
        mode &= ~I8042_MODE_DISABLE_I8042;
        update_irq();
        break;
    case I8042_CCMD_READ_INPORT:
        queue(0x00, 0);
        break;
    case I8042_CCMD_READ_OUTPORT:
        /* XXX: check that */
	DEBUGGER_ENTER_M("UNIMPLEMENTED GET A20");
	//val = 0x01 | (ioport_get_a20() << 1);
	val = 0x01;
	if (status & I8042_STAT_OBF)
            val |= 0x10;
        if (status & I8042_STAT_MOUSE_OBF)
            val |= 0x20;
        queue(val, 0);
        break;
    case I8042_CCMD_ENABLE_A20:
	DEBUGGER_ENTER_M("UNIMPLEMENTED SET A20");
        //ioport_set_a20(1);
        break;
    case I8042_CCMD_DISABLE_A20:
	DEBUGGER_ENTER_M("UNIMPLEMENTED SET A20");
        //ioport_set_a20(0);
        break;
    case I8042_CCMD_RESET:
	backend_reboot();
        break;
    case 0xff:
        /* ignore that - I don't know what is its use */
        break;
    default:
        printf("unsupported keyboard cmd=0x%02x\n", val);
        break;
    }
}

u32_t i8042_t::read()
{
    u32_t val = (pending == I8042_PENDING_AUX) ?
	ps2_read_mouse_data() : ps2_read_kbd_data();
    
    dprintf(debug_i8042, "i8042 read data %x\n", val);
    return val;
}

void i8042_t::write(u32_t val)
{
    dprintf(debug_i8042, "i8042 write data %x\n", val);

    switch(write_cmd) {
    case 0:
        ps2_write_keyboard(val);
        break;
    case I8042_CCMD_WRITE_MODE:
        mode = val;
        ps2_keyboard_set_translation((mode & I8042_MODE_KCC) != 0);
        /* ??? */
        update_irq();
        break;
    case I8042_CCMD_WRITE_OBUF:
        queue(val, 0);
        break;
    case I8042_CCMD_WRITE_AUX_OBUF:
        queue(val, 1);
        break;
    case I8042_CCMD_WRITE_OUTPORT:
	DEBUGGER_ENTER_M("UNIMPLEMENTED SET A20");
        //ioport_set_a20((val >> 1) & 1);
        if (!(val & 1)) {
            backend_reboot();
        }
        break;
    case I8042_CCMD_WRITE_MOUSE:
        ps2_write_mouse(val);
        break;
    default:
        break;
    }
    write_cmd = 0;
}

void i8042_t::reset()
{
    mode = I8042_MODE_I8042_INT | I8042_MODE_MOUSE_INT;
    status = I8042_STAT_CMD | I8042_STAT_UNLOCKED;
}

i8042_t i8042;

void i8042_t::update_i8042_irq(int level)
{
    if (level)
        i8042.pending |= I8042_PENDING_I8042;
    else
        i8042.pending &= ~I8042_PENDING_I8042;
    i8042.update_irq();
}

void i8042_t::update_i8042_aux_irq(int level)
{
    if (level)
        i8042.pending |= I8042_PENDING_AUX;
    else
        i8042.pending &= ~I8042_PENDING_AUX;
    
    i8042.update_irq();
}

void i8042_portio( u16_t port, u32_t &value, bool read )
{
    switch(port)
    {
    case 0x60:
	if( read )
	    value = i8042.read();
	else 
	    i8042.write(value);
	break;
    case 0x64:
	if( read ) 
	    value = i8042.read_status();
	else 
	    i8042.write_command(value);
	break;
    default:
	break;
    }
}


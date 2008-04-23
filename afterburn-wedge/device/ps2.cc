/*
 * QEMU PS/2 keyboard/mouse emulation
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
//#define DEBUG_KBD

/* debug PC keyboard : only mouse */
//#define DEBUG_MOUSE

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_ECHO     	0xEE
#define KBD_CMD_GET_ID 	        0xF2	/* get keyboard ID */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE   	0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

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

#define PS2_QUEUE_SIZE 256

#include INC_ARCH(types.h)
#include INC_ARCH(intlogic.h)

typedef struct {
    u8_t data[PS2_QUEUE_SIZE];
    int rptr, wptr, count;
} PS2Queue;

typedef struct {
    PS2Queue queue;
    s32_t write_cmd;
    void (*update_irq)(int);
} PS2State;

typedef struct {
    PS2State common;
    int scan_enabled;
    /* Qemu uses translated PC scancodes internally.  To avoid multiple
       conversions we do the translation (if any) in the PS/2 emulation
       not the keyboard controller.  */
    int translate;
} PS2KbdState;

typedef struct {
    PS2State common;
    u8_t mouse_status;
    u8_t mouse_resolution;
    u8_t mouse_sample_rate;
    u8_t mouse_wrap;
    u8_t mouse_type; /* 0 = PS2, 3 = IMPS/2, 4 = IMEX */
    u8_t mouse_detect_state;
    int mouse_dx; /* current values, needed for 'poll' mode */
    int mouse_dy;
    int mouse_dz;
    u8_t mouse_buttons;
} PS2MouseState;

PS2MouseState ps2mouse;
PS2KbdState ps2kbd;

/* Table to convert from PC scancodes to raw scancodes.  */
static const unsigned char ps2_raw_keycode[128] = {
          0,118, 22, 30, 38, 37, 46, 54, 61, 62, 70, 69, 78, 85,102, 13,
         21, 29, 36, 45, 44, 53, 60, 67, 68, 77, 84, 91, 90, 20, 28, 27,
         35, 43, 52, 51, 59, 66, 75, 76, 82, 14, 18, 93, 26, 34, 33, 42,
         50, 49, 58, 65, 73, 74, 89,124, 17, 41, 88,  5,  6,  4, 12,  3,
         11,  2, 10,  1,  9,119,126,108,117,125,123,107,115,116,121,105,
        114,122,112,113,127, 96, 97,120,  7, 15, 23, 31, 39, 47, 55, 63,
         71, 79, 86, 94,  8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 87,111,
         19, 25, 57, 81, 83, 92, 95, 98, 99,100,101,103,104,106,109,110
};

void ps2_queue_kbd(int b)
{
    PS2State *s = &ps2kbd.common;
    
    PS2Queue *q = &s->queue;

    if (q->count >= PS2_QUEUE_SIZE)
        return;
    q->data[q->wptr] = b;
    if (++q->wptr == PS2_QUEUE_SIZE)
        q->wptr = 0;
    q->count++;
    s->update_irq(1);
}

void ps2_queue_mouse(int b)
{
    PS2State *s = &ps2mouse.common;
    
    PS2Queue *q = &s->queue;

    if (q->count >= PS2_QUEUE_SIZE)
        return;
    q->data[q->wptr] = b;
    if (++q->wptr == PS2_QUEUE_SIZE)
        q->wptr = 0;
    q->count++;
    s->update_irq(1);
}

UNUSED static void ps2_put_keycode(int keycode)
{
    if (!ps2kbd.translate && keycode < 0xe0)
      {
        if (keycode & 0x80)
            ps2_queue_kbd(0xf0);
        keycode = ps2_raw_keycode[keycode & 0x7f];
      }
    ps2_queue_kbd(keycode);
}

u32_t ps2_read_data(PS2State *s)
{
    PS2Queue *q;
    int val, index;
    
    q = &s->queue;
    if (q->count == 0) {
        /* NOTE: if no data left, we return the last keyboard one
           (needed for EMM386) */
        /* XXX: need a timer to do things correctly */
        index = q->rptr - 1;
        if (index < 0)
            index = PS2_QUEUE_SIZE - 1;
        val = q->data[index];
    } else {
        val = q->data[q->rptr];
        if (++q->rptr == PS2_QUEUE_SIZE)
            q->rptr = 0;
        q->count--;
        /* reading deasserts IRQ */
        s->update_irq(0);
        /* reassert IRQs if data left */
        s->update_irq(q->count != 0);
    }
    return val;
}

u32_t ps2_read_kbd_data()
{
    return ps2_read_data(&ps2kbd.common);
}

u32_t ps2_read_mouse_data()
{
    return ps2_read_data(&ps2mouse.common);
}

static void ps2_reset_keyboard()
{
    ps2kbd.scan_enabled = 1;
}

void ps2_write_keyboard(int val)
{
    
    switch(ps2kbd.common.write_cmd) {
    default:
    case -1:
        switch(val) {
        case 0x00:
            ps2_queue_kbd(KBD_REPLY_ACK);
            break;
        case 0x05:
            ps2_queue_kbd(KBD_REPLY_RESEND);
            break;
        case KBD_CMD_GET_ID:
            ps2_queue_kbd(KBD_REPLY_ACK);
            ps2_queue_kbd(0xab);
            ps2_queue_kbd(0x83);
            break;
        case KBD_CMD_ECHO:
            ps2_queue_kbd(KBD_CMD_ECHO);
            break;
        case KBD_CMD_ENABLE:
            ps2kbd.scan_enabled = 1;
            ps2_queue_kbd(KBD_REPLY_ACK);
            break;
        case KBD_CMD_SET_LEDS:
        case KBD_CMD_SET_RATE:
            ps2kbd.common.write_cmd = val;
            ps2_queue_kbd(KBD_REPLY_ACK);
            break;
        case KBD_CMD_RESET_DISABLE:
            ps2_reset_keyboard();
            ps2kbd.scan_enabled = 0;
            ps2_queue_kbd(KBD_REPLY_ACK);
            break;
        case KBD_CMD_RESET_ENABLE:
            ps2_reset_keyboard();
            ps2kbd.scan_enabled = 1;
            ps2_queue_kbd(KBD_REPLY_ACK);
            break;
        case KBD_CMD_RESET:
            ps2_reset_keyboard();
            ps2_queue_kbd(KBD_REPLY_ACK);
            ps2_queue_kbd(KBD_REPLY_POR);
            break;
        default:
            ps2_queue_kbd(KBD_REPLY_ACK);
            break;
        }
        break;
    case KBD_CMD_SET_LEDS:
        ps2_queue_kbd(KBD_REPLY_ACK);
        ps2kbd.common.write_cmd = -1;
        break;
    case KBD_CMD_SET_RATE:
        ps2_queue_kbd(KBD_REPLY_ACK);
        ps2kbd.common.write_cmd = -1;
        break;
    }
}

/* Set the scancode translation mode.
   0 = raw scancodes.
   1 = translated scancodes (used by qemu internally).  */

void ps2_keyboard_set_translation(int mode)
{
    ps2kbd.translate = mode;
}

static void ps2_mouse_send_packet()
{
    unsigned int b;
    int dx1, dy1, dz1;

    dx1 = ps2mouse.mouse_dx;
    dy1 = ps2mouse.mouse_dy;
    dz1 = ps2mouse.mouse_dz;
    /* XXX: increase range to 8 bits ? */
    if (dx1 > 127)
        dx1 = 127;
    else if (dx1 < -127)
        dx1 = -127;
    if (dy1 > 127)
        dy1 = 127;
    else if (dy1 < -127)
        dy1 = -127;
    b = 0x08 | ((dx1 < 0) << 4) | ((dy1 < 0) << 5) | (ps2mouse.mouse_buttons & 0x07);
    ps2_queue_mouse(b);
    ps2_queue_mouse(dx1 & 0xff);
    ps2_queue_mouse(dy1 & 0xff);
    /* extra byte for IMPS/2 or IMEX */
    switch(ps2mouse.mouse_type) {
    default:
        break;
    case 3:
        if (dz1 > 127)
            dz1 = 127;
        else if (dz1 < -127)
                dz1 = -127;
        ps2_queue_mouse(dz1 & 0xff);
        break;
    case 4:
        if (dz1 > 7)
            dz1 = 7;
        else if (dz1 < -7)
            dz1 = -7;
        b = (dz1 & 0x0f) | ((ps2mouse.mouse_buttons & 0x18) << 1);
        ps2_queue_mouse(b);
        break;
    }

    /* update deltas */
    ps2mouse.mouse_dx -= dx1;
    ps2mouse.mouse_dy -= dy1;
    ps2mouse.mouse_dz -= dz1;
}

UNUSED static void ps2_mouse_event(void *opaque, 
                            int dx, int dy, int dz, int buttons_state)
{
    /* check if deltas are recorded when disabled */
    if (!(ps2mouse.mouse_status & MOUSE_STATUS_ENABLED))
        return;

    ps2mouse.mouse_dx += dx;
    ps2mouse.mouse_dy -= dy;
    ps2mouse.mouse_dz += dz;
    /* XXX: SDL sometimes generates nul events: we delete them */
    if (ps2mouse.mouse_dx == 0 && ps2mouse.mouse_dy == 0 && ps2mouse.mouse_dz == 0 &&
        ps2mouse.mouse_buttons == buttons_state)
	return;
    ps2mouse.mouse_buttons = buttons_state;
    
    if (!(ps2mouse.mouse_status & MOUSE_STATUS_REMOTE) &&
        (ps2mouse.common.queue.count < (PS2_QUEUE_SIZE - 16))) {
        for(;;) {
            /* if not remote, send event. Multiple events are sent if
               too big deltas */
            ps2_mouse_send_packet();
            if (ps2mouse.mouse_dx == 0 && ps2mouse.mouse_dy == 0 && ps2mouse.mouse_dz == 0)
                break;
        }
    }
}

void ps2_write_mouse(int val)
{
#ifdef DEBUG_MOUSE
    printf("kbd: write mouse 0x%02x\n", val);
#endif
    switch(ps2mouse.common.write_cmd) {
    default:
    case -1:
        /* mouse command */
        if (ps2mouse.mouse_wrap) {
            if (val == AUX_RESET_WRAP) {
                ps2mouse.mouse_wrap = 0;
                ps2_queue_mouse(AUX_ACK);
                return;
            } else if (val != AUX_RESET) {
                ps2_queue_mouse(val);
                return;
            }
        }
        switch(val) {
        case AUX_SET_SCALE11:
            ps2mouse.mouse_status &= ~MOUSE_STATUS_SCALE21;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_SET_SCALE21:
            ps2mouse.mouse_status |= MOUSE_STATUS_SCALE21;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_SET_STREAM:
            ps2mouse.mouse_status &= ~MOUSE_STATUS_REMOTE;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_SET_WRAP:
            ps2mouse.mouse_wrap = 1;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_SET_REMOTE:
            ps2mouse.mouse_status |= MOUSE_STATUS_REMOTE;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_GET_TYPE:
            ps2_queue_mouse(AUX_ACK);
            ps2_queue_mouse(ps2mouse.mouse_type);
            break;
        case AUX_SET_RES:
        case AUX_SET_SAMPLE:
            ps2mouse.common.write_cmd = val;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_GET_SCALE:
            ps2_queue_mouse(AUX_ACK);
            ps2_queue_mouse(ps2mouse.mouse_status);
            ps2_queue_mouse(ps2mouse.mouse_resolution);
            ps2_queue_mouse(ps2mouse.mouse_sample_rate);
            break;
        case AUX_POLL:
            ps2_queue_mouse(AUX_ACK);
            ps2_mouse_send_packet();
            break;
        case AUX_ENABLE_DEV:
            ps2mouse.mouse_status |= MOUSE_STATUS_ENABLED;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_DISABLE_DEV:
            ps2mouse.mouse_status &= ~MOUSE_STATUS_ENABLED;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_SET_DEFAULT:
            ps2mouse.mouse_sample_rate = 100;
            ps2mouse.mouse_resolution = 2;
            ps2mouse.mouse_status = 0;
            ps2_queue_mouse(AUX_ACK);
            break;
        case AUX_RESET:
            ps2mouse.mouse_sample_rate = 100;
            ps2mouse.mouse_resolution = 2;
            ps2mouse.mouse_status = 0;
            ps2mouse.mouse_type = 0;
            ps2_queue_mouse(AUX_ACK);
            ps2_queue_mouse(0xaa);
            ps2_queue_mouse(ps2mouse.mouse_type);
            break;
        default:
            break;
        }
        break;
    case AUX_SET_SAMPLE:
        ps2mouse.mouse_sample_rate = val;
        /* detect IMPS/2 or IMEX */
        switch(ps2mouse.mouse_detect_state) {
        default:
        case 0:
            if (val == 200)
                ps2mouse.mouse_detect_state = 1;
            break;
        case 1:
            if (val == 100)
                ps2mouse.mouse_detect_state = 2;
            else if (val == 200)
                ps2mouse.mouse_detect_state = 3;
            else
                ps2mouse.mouse_detect_state = 0;
            break;
        case 2:
            if (val == 80) 
                ps2mouse.mouse_type = 3; /* IMPS/2 */
            ps2mouse.mouse_detect_state = 0;
            break;
        case 3:
            if (val == 80) 
                ps2mouse.mouse_type = 4; /* IMEX */
            ps2mouse.mouse_detect_state = 0;
            break;
        }
        ps2_queue_mouse(AUX_ACK);
        ps2mouse.common.write_cmd = -1;
        break;
    case AUX_SET_RES:
        ps2mouse.mouse_resolution = val;
        ps2_queue_mouse(AUX_ACK);
        ps2mouse.common.write_cmd = -1;
        break;
    }
}

static void ps2_reset(void *opaque)
{
    PS2State *s = (PS2State *)opaque;
    PS2Queue *q;
    s->write_cmd = -1;
    q = &s->queue;
    q->rptr = 0;
    q->wptr = 0;
    q->count = 0;
}

void ps2_kbd_init(void (*update_irq)(int))
{

    ps2kbd.common.update_irq = update_irq;
    ps2_reset(&ps2kbd.common);
}

void ps2_mouse_init(void (*update_irq)(int))
{
    ps2mouse.common.update_irq = update_irq;
    ps2_reset(&ps2mouse.common);
}

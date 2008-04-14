/*********************************************************************
 *                
 * Copyright (C) 2008,  Karlsruhe University
 *                
 * File path:     device/i8042.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __DEVICE__I8042_H__
#define __DEVICE__I8042_H__

void ps2_kbd_init(void (*update_irq)(int));
void ps2_mouse_init(void (*update_irq)(int));
void ps2_write_mouse(int val);
void ps2_write_keyboard(int val);
u32_t ps2_read_kbd_data();
u32_t ps2_read_mouse_data();
void ps2_queue_kbd(int b);
void ps2_queue_mouse(int b);
void ps2_keyboard_set_translation(int mode);

#endif /* !__DEVICE__I8042_H__ */

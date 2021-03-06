/*
 * QEMU USB HID devices
 * 
 * Copyright (c) 2005 Fabrice Bellard
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
#include "vl.h"

/* HID interface requests */
#define GET_REPORT   0xa101
#define GET_IDLE     0xa102
#define GET_PROTOCOL 0xa103
#define SET_IDLE     0x210a
#define SET_PROTOCOL 0x210b

#define USB_MOUSE  1
#define USB_TABLET 2

typedef struct USBMouseState {
    USBDevice dev;
    int dx, dy, dz, buttons_state;
    int x, y;
    int kind;
    int mouse_grabbed;
    int status_changed;
    QEMUPutMouseEntry *eh_entry;
} USBMouseState;

/* mostly the same values as the Bochs USB Mouse device */
static const uint8_t qemu_mouse_dev_descriptor[] = {
	0x12,       /*  u8 bLength; */
	0x01,       /*  u8 bDescriptorType; Device */
	0x10, 0x00, /*  u16 bcdUSB; v1.0 */

	0x00,	    /*  u8  bDeviceClass; */
	0x00,	    /*  u8  bDeviceSubClass; */
	0x00,       /*  u8  bDeviceProtocol; [ low/full speeds only ] */
	0x08,       /*  u8  bMaxPacketSize0; 8 Bytes */

	0x27, 0x06, /*  u16 idVendor; */
 	0x01, 0x00, /*  u16 idProduct; */
	0x00, 0x00, /*  u16 bcdDevice */

	0x03,       /*  u8  iManufacturer; */
	0x02,       /*  u8  iProduct; */
	0x01,       /*  u8  iSerialNumber; */
	0x01        /*  u8  bNumConfigurations; */
};

static const uint8_t qemu_mouse_config_descriptor[] = {
	/* one configuration */
	0x09,       /*  u8  bLength; */
	0x02,       /*  u8  bDescriptorType; Configuration */
	0x22, 0x00, /*  u16 wTotalLength; */
	0x01,       /*  u8  bNumInterfaces; (1) */
	0x01,       /*  u8  bConfigurationValue; */
	0x04,       /*  u8  iConfiguration; */
	0xa0,       /*  u8  bmAttributes; 
				 Bit 7: must be set,
				     6: Self-powered,
				     5: Remote wakeup,
				     4..0: resvd */
	50,         /*  u8  MaxPower; */
      
	/* USB 1.1:
	 * USB 2.0, single TT organization (mandatory):
	 *	one interface, protocol 0
	 *
	 * USB 2.0, multiple TT organization (optional):
	 *	two interfaces, protocols 1 (like single TT)
	 *	and 2 (multiple TT mode) ... config is
	 *	sometimes settable
	 *	NOT IMPLEMENTED
	 */

	/* one interface */
	0x09,       /*  u8  if_bLength; */
	0x04,       /*  u8  if_bDescriptorType; Interface */
	0x00,       /*  u8  if_bInterfaceNumber; */
	0x00,       /*  u8  if_bAlternateSetting; */
	0x01,       /*  u8  if_bNumEndpoints; */
	0x03,       /*  u8  if_bInterfaceClass; */
	0x01,       /*  u8  if_bInterfaceSubClass; */
	0x02,       /*  u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
	0x05,       /*  u8  if_iInterface; */
     
        /* HID descriptor */
        0x09,        /*  u8  bLength; */
        0x21,        /*  u8 bDescriptorType; */
        0x01, 0x00,  /*  u16 HID_class */
        0x00,        /*  u8 country_code */
        0x01,        /*  u8 num_descriptors */
        0x22,        /*  u8 type; Report */
        50, 0,       /*  u16 len */

	/* one endpoint (status change endpoint) */
	0x07,       /*  u8  ep_bLength; */
	0x05,       /*  u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x03,       /*  u8  ep_bmAttributes; Interrupt */
 	0x03, 0x00, /*  u16 ep_wMaxPacketSize; */
	0x0a,       /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
};

static const uint8_t qemu_tablet_config_descriptor[] = {
	/* one configuration */
	0x09,       /*  u8  bLength; */
	0x02,       /*  u8  bDescriptorType; Configuration */
	0x22, 0x00, /*  u16 wTotalLength; */
	0x01,       /*  u8  bNumInterfaces; (1) */
	0x01,       /*  u8  bConfigurationValue; */
	0x04,       /*  u8  iConfiguration; */
	0xa0,       /*  u8  bmAttributes; 
				 Bit 7: must be set,
				     6: Self-powered,
				     5: Remote wakeup,
				     4..0: resvd */
	50,         /*  u8  MaxPower; */
      
	/* USB 1.1:
	 * USB 2.0, single TT organization (mandatory):
	 *	one interface, protocol 0
	 *
	 * USB 2.0, multiple TT organization (optional):
	 *	two interfaces, protocols 1 (like single TT)
	 *	and 2 (multiple TT mode) ... config is
	 *	sometimes settable
	 *	NOT IMPLEMENTED
	 */

	/* one interface */
	0x09,       /*  u8  if_bLength; */
	0x04,       /*  u8  if_bDescriptorType; Interface */
	0x00,       /*  u8  if_bInterfaceNumber; */
	0x00,       /*  u8  if_bAlternateSetting; */
	0x01,       /*  u8  if_bNumEndpoints; */
	0x03,       /*  u8  if_bInterfaceClass; */
	0x01,       /*  u8  if_bInterfaceSubClass; */
	0x02,       /*  u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
	0x05,       /*  u8  if_iInterface; */

        /* HID descriptor */
        0x09,        /*  u8  bLength; */
        0x21,        /*  u8 bDescriptorType; */
        0x01, 0x00,  /*  u16 HID_class */
        0x00,        /*  u8 country_code */
        0x01,        /*  u8 num_descriptors */
        0x22,        /*  u8 type; Report */
        74, 0,       /*  u16 len */

	/* one endpoint (status change endpoint) */
	0x07,       /*  u8  ep_bLength; */
	0x05,       /*  u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x03,       /*  u8  ep_bmAttributes; Interrupt */
 	0x08, 0x00, /*  u16 ep_wMaxPacketSize; */
	0x03,       /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
};

static const uint8_t qemu_mouse_hid_report_descriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 
    0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 
    0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 
    0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
    0xC0, 0xC0,
};

static const uint8_t qemu_tablet_hid_report_descriptor[] = {
        0x05, 0x01, /* Usage Page Generic Desktop */
        0x09, 0x01, /* Usage Mouse */
        0xA1, 0x01, /* Collection Application */
        0x09, 0x01, /* Usage Pointer */
        0xA1, 0x00, /* Collection Physical */
        0x05, 0x09, /* Usage Page Button */
        0x19, 0x01, /* Usage Minimum Button 1 */
        0x29, 0x03, /* Usage Maximum Button 3 */
        0x15, 0x00, /* Logical Minimum 0 */
        0x25, 0x01, /* Logical Maximum 1 */
        0x95, 0x03, /* Report Count 3 */
        0x75, 0x01, /* Report Size 1 */
        0x81, 0x02, /* Input (Data, Var, Abs) */
        0x95, 0x01, /* Report Count 1 */
        0x75, 0x05, /* Report Size 5 */
        0x81, 0x01, /* Input (Cnst, Var, Abs) */
        0x05, 0x01, /* Usage Page Generic Desktop */
        0x09, 0x30, /* Usage X */
        0x09, 0x31, /* Usage Y */
        0x15, 0x00, /* Logical Minimum 0 */
        0x26, 0xFF, 0x7F, /* Logical Maximum 0x7fff */
        0x35, 0x00, /* Physical Minimum 0 */
        0x46, 0xFE, 0x7F, /* Physical Maximum 0x7fff */
        0x75, 0x10, /* Report Size 16 */
        0x95, 0x02, /* Report Count 2 */
        0x81, 0x02, /* Input (Data, Var, Abs) */
        0x05, 0x01, /* Usage Page Generic Desktop */
        0x09, 0x38, /* Usage Wheel */
        0x15, 0x81, /* Logical Minimum -127 */
        0x25, 0x7F, /* Logical Maximum 127 */
        0x35, 0x00, /* Physical Minimum 0 (same as logical) */
        0x45, 0x00, /* Physical Maximum 0 (same as logical) */
        0x75, 0x08, /* Report Size 8 */
        0x95, 0x01, /* Report Count 1 */
        0x81, 0x02, /* Input (Data, Var, Rel) */
        0xC0,       /* End Collection */
        0xC0,       /* End Collection */
};

static int currentbutton = 0;
typedef struct _mouseclick {
    int button_state;
    struct _mouseclick *next;
} mouseclick; 
static mouseclick mousequeue[20];
static mouseclick *head = mousequeue;
static mouseclick *tail = mousequeue;

static void usb_mouse_event(void *opaque,
                            int dx1, int dy1, int dz1, int buttons_state)
{
    USBMouseState *s = opaque;

    if (s->status_changed == 1){
        //A mouse event is lost
        if (buttons_state != currentbutton && tail->next != head) {
            //A left click event is lost: let's add it to the queue
            //counter++;
            tail->button_state = buttons_state;
            tail = tail->next;
        }
    }
    else {
        s->buttons_state = buttons_state;
    }

    s->dx += dx1;
    s->dy += dy1;
    s->dz += dz1;
    currentbutton = buttons_state;
    s->status_changed = 1;
}

static void usb_tablet_event(void *opaque,
			     int x, int y, int dz, int buttons_state)
{
    USBMouseState *s = opaque;
    
    if (s->status_changed == 1){
        //A mouse event is lost
        if (buttons_state != currentbutton && tail->next != head) {
            //A left click event is lost: let's add it to the queue
            //counter++;
            tail->button_state = buttons_state;
            tail = tail->next;
        }
    }
    else {
        s->buttons_state = buttons_state;
    }

    s->x = x;
    s->y = y;
    s->dz += dz;
    currentbutton = buttons_state;
    s->status_changed = 1;
}

static inline int int_clamp(int val, int vmin, int vmax)
{
    if (val < vmin)
        return vmin;
    else if (val > vmax)
        return vmax;
    else
        return val;
}

static int usb_mouse_poll(USBMouseState *s, uint8_t *buf, int len)
{
    int dx, dy, dz, b, l;

    if (!s->mouse_grabbed) {
	s->eh_entry = qemu_add_mouse_event_handler(usb_mouse_event, s,
                                                  0, "QEMU USB Mouse");
	s->mouse_grabbed = 1;
    }
    
    dx = int_clamp(s->dx, -128, 127);
    dy = int_clamp(s->dy, -128, 127);
    dz = int_clamp(s->dz, -128, 127);

    s->dx -= dx;
    s->dy -= dy;
    s->dz -= dz;
    
    b = 0;
    if (s->buttons_state & MOUSE_EVENT_LBUTTON)
        b |= 0x01;
    if (s->buttons_state & MOUSE_EVENT_RBUTTON)
        b |= 0x02;
    if (s->buttons_state & MOUSE_EVENT_MBUTTON)
        b |= 0x04;
    
    buf[0] = b;
    buf[1] = dx;
    buf[2] = dy;
    l = 3;
    if (len >= 4) {
        buf[3] = dz;
        l = 4;
    }
    return l;
}

static int usb_tablet_poll(USBMouseState *s, uint8_t *buf, int len)
{
    int dz, b, l;

    if (!s->mouse_grabbed) {
	s->eh_entry = qemu_add_mouse_event_handler(usb_tablet_event, s,
                                                  1, "QEMU USB Tablet");
	s->mouse_grabbed = 1;
    }
    
    dz = int_clamp(s->dz, -128, 127);
    s->dz -= dz;

    /* Appears we have to invert the wheel direction */
    dz = 0 - dz;
    b = 0;
    if (s->buttons_state & MOUSE_EVENT_LBUTTON)
        b |= 0x01;
    if (s->buttons_state & MOUSE_EVENT_RBUTTON)
        b |= 0x02;
    if (s->buttons_state & MOUSE_EVENT_MBUTTON)
        b |= 0x04;

    buf[0] = b;
    buf[1] = s->x & 0xff;
    buf[2] = s->x >> 8;
    buf[3] = s->y & 0xff;
    buf[4] = s->y >> 8;
    buf[5] = dz;
    l = 6;

    return l;
}

static void usb_mouse_handle_reset(USBDevice *dev)
{
    USBMouseState *s = (USBMouseState *)dev;

    s->dx = 0;
    s->dy = 0;
    s->dz = 0;
    s->x = 0;
    s->y = 0;
    s->buttons_state = 0;
}

static int usb_mouse_handle_control(USBDevice *dev, int request, int value,
                                  int index, int length, uint8_t *data)
{
    USBMouseState *s = (USBMouseState *)dev;
    int ret = 0;

    switch(request) {
    case DeviceRequest | USB_REQ_GET_STATUS:
        data[0] = (1 << USB_DEVICE_SELF_POWERED) |
            (dev->remote_wakeup << USB_DEVICE_REMOTE_WAKEUP);
        data[1] = 0x00;
        ret = 2;
        break;
    case DeviceOutRequest | USB_REQ_CLEAR_FEATURE:
        if (value == USB_DEVICE_REMOTE_WAKEUP) {
            dev->remote_wakeup = 0;
        } else {
            goto fail;
        }
        ret = 0;
        break;
    case DeviceOutRequest | USB_REQ_SET_FEATURE:
        if (value == USB_DEVICE_REMOTE_WAKEUP) {
            dev->remote_wakeup = 1;
        } else {
            goto fail;
        }
        ret = 0;
        break;
    case DeviceOutRequest | USB_REQ_SET_ADDRESS:
        dev->addr = value;
        ret = 0;
        break;
    case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
        switch(value >> 8) {
        case USB_DT_DEVICE:
            memcpy(data, qemu_mouse_dev_descriptor, 
                   sizeof(qemu_mouse_dev_descriptor));
            ret = sizeof(qemu_mouse_dev_descriptor);
            break;
        case USB_DT_CONFIG:
	    if (s->kind == USB_MOUSE) {
		memcpy(data, qemu_mouse_config_descriptor, 
		       sizeof(qemu_mouse_config_descriptor));
		ret = sizeof(qemu_mouse_config_descriptor);
	    } else if (s->kind == USB_TABLET) {
		memcpy(data, qemu_tablet_config_descriptor, 
		       sizeof(qemu_tablet_config_descriptor));
		ret = sizeof(qemu_tablet_config_descriptor);
	    }		
            break;
        case USB_DT_STRING:
            switch(value & 0xff) {
            case 0:
                /* language ids */
                data[0] = 4;
                data[1] = 3;
                data[2] = 0x09;
                data[3] = 0x04;
                ret = 4;
                break;
            case 1:
                /* serial number */
                ret = set_usb_string(data, "1");
                break;
            case 2:
                /* product description */
		if (s->kind == USB_MOUSE)
		    ret = set_usb_string(data, "QEMU USB Mouse");
		else if (s->kind == USB_TABLET)
		    ret = set_usb_string(data, "QEMU USB Tablet");
                break;
            case 3:
                /* vendor description */
                ret = set_usb_string(data, "QEMU " QEMU_VERSION);
                break;
            case 4:
                ret = set_usb_string(data, "HID Mouse");
                break;
            case 5:
                ret = set_usb_string(data, "Endpoint1 Interrupt Pipe");
                break;
            default:
                goto fail;
            }
            break;
        default:
            goto fail;
        }
        break;
    case DeviceRequest | USB_REQ_GET_CONFIGURATION:
        data[0] = 1;
        ret = 1;
        break;
    case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
        ret = 0;
        break;
    case DeviceRequest | USB_REQ_GET_INTERFACE:
        data[0] = 0;
        ret = 1;
        break;
    case DeviceOutRequest | USB_REQ_SET_INTERFACE:
        ret = 0;
        break;
        /* hid specific requests */
    case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
        switch(value >> 8) {
        case 0x22:
	    if (s->kind == USB_MOUSE) {
		memcpy(data, qemu_mouse_hid_report_descriptor, 
		       sizeof(qemu_mouse_hid_report_descriptor));
		ret = sizeof(qemu_mouse_hid_report_descriptor);
	    } else if (s->kind == USB_TABLET) {
		memcpy(data, qemu_tablet_hid_report_descriptor, 
		       sizeof(qemu_tablet_hid_report_descriptor));
		ret = sizeof(qemu_tablet_hid_report_descriptor);
	    }
	    break;
        default:
            goto fail;
        }
        break;
    case GET_REPORT:
	if (s->kind == USB_MOUSE)
	    ret = usb_mouse_poll(s, data, length);
	else if (s->kind == USB_TABLET)
	    ret = usb_tablet_poll(s, data, length);
        break;
    case SET_IDLE:
        ret = 0;
        break;
    default:
    fail:
        ret = USB_RET_STALL;
        break;
    }
    return ret;
}

static int usb_mouse_handle_data(USBDevice *dev, USBPacket *p)
{
    USBMouseState *s = (USBMouseState *)dev;
    int ret = 0;

    switch(p->pid) {
    case USB_TOKEN_IN:
        if (p->devep == 1) {
            if (s->kind == USB_MOUSE)
                ret = usb_mouse_poll(s, p->data, p->len);
            else if (s->kind == USB_TABLET)
                ret = usb_tablet_poll(s, p->data, p->len);

            if (!s->status_changed) {
                ret = USB_RET_NAK;
            } else {
                if (head != tail) {
                    s->buttons_state = head->button_state;
                    head = head->next;
                }
                else {
                    s->status_changed = 0;
                }
            }

        } else {
            goto fail;
        }
        break;
    case USB_TOKEN_OUT:
    default:
    fail:
        ret = USB_RET_STALL;
        break;
    }
    return ret;
}

static void usb_mouse_handle_destroy(USBDevice *dev)
{
    USBMouseState *s = (USBMouseState *)dev;

    qemu_remove_mouse_event_handler(s->eh_entry);
    qemu_free(s);
}

void usb_mouse_save(QEMUFile *f, void *opaque)
{
    USBMouseState *s = (USBMouseState*)opaque;

    qemu_put_be32s(f, &s->dx);
    qemu_put_be32s(f, &s->dy);
    qemu_put_be32s(f, &s->dz);
    qemu_put_be32s(f, &s->buttons_state);
    qemu_put_be32s(f, &s->x);
    qemu_put_be32s(f, &s->y);
    qemu_put_be32s(f, &s->kind);
    qemu_put_be32s(f, &s->mouse_grabbed);
    qemu_put_be32s(f, &s->status_changed);

}

int usb_mouse_load(QEMUFile *f, void *opaque, int version_id)
{
    USBMouseState *s = (USBMouseState*)opaque;

    if (version_id != 1)
        return -EINVAL;

    qemu_get_be32s(f, &s->dx);
    qemu_get_be32s(f, &s->dy);
    qemu_get_be32s(f, &s->dz);
    qemu_get_be32s(f, &s->buttons_state);
    qemu_get_be32s(f, &s->x);
    qemu_get_be32s(f, &s->y);
    qemu_get_be32s(f, &s->kind);
    qemu_get_be32s(f, &s->mouse_grabbed);
    qemu_get_be32s(f, &s->status_changed);

    if ( s->kind == USB_TABLET) {
        fprintf(logfile, "usb_mouse_load:add usb_tablet_event.\n");
        qemu_add_mouse_event_handler(usb_tablet_event, s, 1, "QEMU USB Tablet");
    } else if ( s->kind == USB_MOUSE) {
        fprintf(logfile, "usb_mouse_load:add usb_mouse_event.\n");
        qemu_add_mouse_event_handler(usb_mouse_event, s, 0, "QEMU USB MOUSE");
    }

    return 0;
}


USBDevice *usb_tablet_init(void)
{
    USBMouseState *s;
    int i;
    
    for (i = 0; i < 19; i++) {
        mousequeue[i].button_state = 0;
        mousequeue[i].next = &(mousequeue[i + 1]);
    }
    mousequeue[i].button_state = 0;
    mousequeue[i].next = mousequeue;

    s = qemu_mallocz(sizeof(USBMouseState));
    if (!s)
        return NULL;
    s->dev.speed = USB_SPEED_FULL;
    s->dev.handle_packet = usb_generic_handle_packet;

    s->dev.handle_reset = usb_mouse_handle_reset;
    s->dev.handle_control = usb_mouse_handle_control;
    s->dev.handle_data = usb_mouse_handle_data;
    s->dev.handle_destroy = usb_mouse_handle_destroy;
    s->kind = USB_TABLET;
    s->status_changed = 0;

    pstrcpy(s->dev.devname, sizeof(s->dev.devname), "QEMU USB Tablet");

    register_savevm("USB tablet dev", 0, 1, usb_mouse_save, usb_mouse_load, s);

    return (USBDevice *)s;
}

USBDevice *usb_mouse_init(void)
{
    USBMouseState *s;
    int i;
    
    for (i = 0; i < 19; i++) {
        mousequeue[i].button_state = 0;
        mousequeue[i].next = &(mousequeue[i + 1]);
    }
    mousequeue[i].button_state = 0;
    mousequeue[i].next = mousequeue;

    s = qemu_mallocz(sizeof(USBMouseState));
    if (!s)
        return NULL;
    s->dev.speed = USB_SPEED_FULL;
    s->dev.handle_packet = usb_generic_handle_packet;

    s->dev.handle_reset = usb_mouse_handle_reset;
    s->dev.handle_control = usb_mouse_handle_control;
    s->dev.handle_data = usb_mouse_handle_data;
    s->dev.handle_destroy = usb_mouse_handle_destroy;
    s->kind = USB_MOUSE;
    s->status_changed = 0;

    pstrcpy(s->dev.devname, sizeof(s->dev.devname), "QEMU USB Mouse");

    register_savevm("USB mouse dev", 0, 1, usb_mouse_save, usb_mouse_load, s);

    return (USBDevice *)s;
}

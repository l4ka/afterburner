/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/device/pci.h
 * Description:   
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: pci.h,v 1.3 2005/04/13 15:47:31 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__DEVICE__PCI_H__
#define __AFTERBURN_WEDGE__INCLUDE__DEVICE__PCI_H__

#include INC_ARCH(types.h)

struct pci_config_addr_t {
    union {
	u32_t raw;
	struct {
	    u32_t zero : 2;
	    u32_t reg  : 6;
	    u32_t func : 3;
	    u32_t dev  : 5;
	    u32_t bus  : 8;
	    u32_t reserved : 7;
	    u32_t enabled : 1;

	} fields;
    } x;

    bool is_enabled() { return x.fields.enabled; }
} __attribute__((packed));

struct pci_command_t
{
    union {
	u16_t raw;
	struct {
	    u16_t io_space_enabled : 1;
	    u16_t mem_space_enabled : 1;
	    u16_t bus_master_enabled : 1;
	    u16_t special_cycles_enabled : 1;
	    u16_t memory_write_ctrl : 1;
	    u16_t vga_snoop_ctrl : 1;
	    u16_t parity_error_response : 1;
	    u16_t wait_cycle_ctrl : 1;
	    u16_t system_err_ctrl : 1;
	    u16_t fast_b2b_ctrl : 1;
	    u16_t reserved : 6;
	} fields;
    } x;
} __attribute__((packed));

struct pci_status_t
{
    union {
	u16_t raw;
	struct {
	    u16_t reserved : 5;
	    u16_t cap_66 : 1;
	    u16_t user_features : 1;
	    u16_t fast_b2b : 1;
	    u16_t data_parity : 1;
	    u16_t device_select_timing : 2;
	    u16_t signaled_target_abort : 1;
	    u16_t received_target_abort : 1;
	    u16_t received_master_abort : 1;
	    u16_t signaled_sys_err : 1;
	    u16_t detected_parity_err : 1;
	} fields;
    } x;
} __attribute__((packed));

struct pci_bist_t {
    union {
	u8_t raw;
	struct {
	    u8_t completion_status : 4;
	    u8_t reserved : 2;
	    u8_t start_ctrl : 1;
	    u8_t supported : 1;
	} fields;
    } x;
} __attribute__((packed));

struct pci_base_addr_t {
    union {
	u32_t raw;
	struct {
	    u32_t io_request : 1;
	    u32_t reserved : 1;
	    u32_t address : 30;
	} io;
	struct {
	    u32_t mem_request : 1;
	    u32_t type : 2;
	    u32_t prefetchable : 1;
	    u32_t address : 28;
	} memory;
    } x;

    enum mem_type_e { normal=0, below_1MB=1, is_64bit=2, reserved=3 };

    bool is_io_address() { return x.io.io_request == 1; }
    bool is_mem_address() { return x.memory.mem_request == 0; }
    bool is_mem_64bit() { return x.memory.type == is_64bit; }

    word_t get_mem_address() { return x.raw & ~0xf; }
    word_t get_io_address()  { return x.raw & ~0x3; }

} __attribute__((packed));

struct pci_header_t
{
    enum base_class_e {
	legacy=0x0, mass_storage=0x1, network=0x2, display=0x3,
	multimedia=0x4, memory=0x5, pci_bridge=0x6, communications=0x7,
	generic_sys=0x8, input=0x9, docking=0xa, processors=0xb,
	serial_bus=0xc, unknown_base_class=0xff,
    };

    enum network_class_e {
	ethernet=0x0, token_ring=0x1, fddi=0x2, atm=0x3, 
	other_network_class=0x80,
    };

    enum pci_bridge_class_e {
	host=0x0, isa=0x1, eisa=0x2, mc=0x3, pci_to_pci=0x4,
	pcmcia=0x5, nubus=0x6, cardbus=0x7,
	other_pci_bridge_class=0x80,
    };

    enum mass_storage_class_e {
	scsi=0x0, ide=0x1, floppy=0x2, raid=0x4,
    };

    union {
	u32_t raw[16];
	struct {
	    u16_t vendor_id;
	    u16_t device_id;
	    pci_command_t command;
	    pci_status_t status;
	    u8_t  revision_id;
	    u8_t  programming_interface;
	    u8_t  sub_class_code;
	    u8_t  base_class_code;
	    u8_t  cache_line_size;
	    u8_t  latency_timer;
	    u8_t  header_type;
	    pci_bist_t  bist;

	    pci_base_addr_t base_addr_registers[6];

	    u32_t cardbus_cis_pointer;
	    u16_t subsys_vendor_id;
	    u16_t subsys_id;
	    u32_t rom_base_addr;
	    u32_t reserved1;
	    u32_t reserved2;
	    u8_t  interrupt_line;
	    u8_t  interrupt_pin;
	    u8_t  min_gnt;
	    u8_t  max_lat;
	} fields;
    } x;

    pci_base_addr_t base_addr_requests[6];

    u32_t read( u32_t base, u32_t offset, u32_t bit_width )
    {
	u32_t mask = (u32_t) ~0;
	if( bit_width < 32 )
	    mask = (1 << bit_width) - 1;
	return mask & (x.raw[ base ] >> offset);
    }

    void write( u32_t base, u32_t offset, u32_t bit_width, u32_t new_value )
    {
	u32_t mask = (u32_t) ~0;
	if( bit_width < 32 )
	    mask = ((1 << bit_width) - 1) << offset;
	x.raw[ base ] = (x.raw[base] & ~mask) | ((new_value << offset) & mask);
    }

    static bool is_base_addr_reg( u32_t base )
	{ return (base >= 4) && (base < (4+6)); }

    void set_base_addr_request( u32_t base )
    { 
	u32_t which = base - 4;
	x.fields.base_addr_registers[ which ] = base_addr_requests[ which ]; 
    }

} __attribute__((packed));

#endif	/* __AFTERBURN_WEDGE__INCLUDE__DEVICE__PCI_H__ */

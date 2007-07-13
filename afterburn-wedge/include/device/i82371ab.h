/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/ide.cc
 * Description:   Front-end emulation for the 82371AB PCI-TO-ISA /
 *                IDE XCELERATOR (PIIX4)
 * Note:          This implementation only covers the IDE Controller part  
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
 ********************************************************************/

#ifndef __DEVICE__I82371AB_H__
#define __DEVICE__I82371AB_H__

#if defined(CONFIG_DEVICE_I82371AB)

#include <device/pci.h>

#include INC_ARCH(sync.h)
#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)

// ide timing register
struct i82371ab_idetim_t {
    union {
	u16_t raw;
	struct {
	    u8_t time0 : 1;
	    u8_t ie0 : 1;
	    u8_t ppe0 : 1;
	    u8_t dte0 : 1;
	    u8_t time1 : 1;
	    u8_t ie1 : 1;
	    u8_t ppe1: 1;
	    u8_t dte1 : 1;
	    u8_t rtc : 2;
	    u8_t reserved : 2;
	    u8_t isp : 2;
	    u8_t sitre : 1;
	    u8_t ide : 1;
	} fields;
    } x;
} __attribute__((packed));

// slave ide timing register
struct i82371ab_sidetim_t {
    union {
	u8_t raw;
	struct {
	    u8_t prtc1 : 2;
	    u8_t pisp1 : 2;
	    u8_t srtc1 : 2;
	    u8_t sisp1 : 2;
	} fields;
    } x;
} __attribute__((packed));

// ultra dma control register
struct i82371ab_udmactl_t {
    union {
	u8_t raw;
	struct {
	    u8_t psde0 : 1;
	    u8_t psde1 : 1;
	    u8_t ssde0 : 1;
	    u8_t ssde1 : 1;
	    u8_t reserved : 4;
	} fields;
    } x;
} __attribute__((packed));

// ultra dma control register
struct i82371ab_udmatim_t {
    union {
	u16_t raw;
	struct {
	    u8_t pct0 : 2;
	    u8_t reserved0 : 2;
	    u8_t pct1 : 2;
	    u8_t reserved1 : 2;
	    u8_t sct0 : 2;
	    u8_t reserved2 : 2;
	    u8_t sct1 : 2;
	    u8_t reserved3 : 2;
	} fields;
    } x;
} __attribute__((packed));

// pci device region
struct i82371ab_pci_t {
    union {
	u32_t raw[48];
	struct {
	    i82371ab_idetim_t pri_tim;
	    i82371ab_idetim_t sec_tim;
	    i82371ab_sidetim_t slave_tim;
	    u8_t reserved0;
	    u8_t reserved1;
	    u8_t reserved2;
	    i82371ab_udmactl_t udma_ctrl;
	    u8_t reserved3;
	    i82371ab_udmatim_t udma_tim;
	} fields;
    }x;
} __attribute__((packed));


// bus master ide command register
struct i82371ab_ioregs_t {

    // bus master ide command register
    union {
	u8_t raw;
	struct {
	    u8_t ssbm: 1;
	    u8_t reserved0 : 2;
	    u8_t rwcon : 1;
	    u8_t reserved1 : 4;
	} fields;
    } bmicx;

    // bus master ide status register
    union {
	u8_t raw;
	struct {
	    u8_t bmidea : 1;
	    u8_t dma_err : 1;
	    u8_t ideints : 1;
	    u8_t reserved0 : 2;
	    u8_t dma0cap : 1;
	    u8_t dma1cap : 1;
	    u8_t reserved : 1;
	} fields;
    } bmisx;

    // descriptor table base address
    u32_t dtba;
};


class i82371ab_t {
 private:
    pci_header_t *pci_header;
    pci_device_t pci_device;
    i82371ab_pci_t *pci_region;

    i82371ab_ioregs_t pri_regs;
    i82371ab_ioregs_t sec_regs;

    void write_register( u16_t port, u32_t &value );
    void read_register ( u16_t port, u32_t &value );

 public:

    i82371ab_t( pci_header_t *new_pci_header ) {
	pci_header = new_pci_header;
	pci_region = (i82371ab_pci_t*)&(pci_device.raw);
	// enable pci i/o
	pci_region->x.fields.pri_tim.x.fields.ide = 1;
	pci_region->x.fields.sec_tim.x.fields.ide = 1;
	// enable dma timing
	pci_region->x.fields.pri_tim.x.fields.dte1 = 1;
	pci_region->x.fields.pri_tim.x.fields.dte0 = 1;
	pci_region->x.fields.sec_tim.x.fields.dte1 = 1;
	pci_region->x.fields.sec_tim.x.fields.dte0 = 1;
    }

    void do_portio( u16_t port, u32_t & value, bool read );

    pci_header_t * get_pci_header() {
	return pci_header;
    }

    pci_device_t *get_pci_device() {
	return &pci_device;
    }

    static const char *get_name() {
	return "i82371ab";
    }

    static i82371ab_t* get_device(word_t index) {
	extern i82371ab_t i82371ab_dev0;
	return &i82371ab_dev0;
    }


};


#endif /* CONFIG_DEVICE_I82371AB */

#endif /* __DEVICE__I82371AB_H__ */

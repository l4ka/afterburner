/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/device/ide.h
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
 ********************************************************************/

#ifndef __DEVICE_IDE_H__
#define __DEVICE_IDE_H__

#include INC_ARCH(types.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(console.h)
#include <L4VMblock_idl_client.h>


#define IDE_MAX_DEVICES 4
#define IDE_MAX_READ_WRITE_MULTIPLE 8
#define IDE_SECTOR_SIZE 512
// must be at least IDE_MAX_READ_WRITE_MULTIPLE * IDE_SECTOR_SIZE !
#define IDE_IOBUFFER_SIZE 65536
#define IDE_IOBUFFER_BLOCKS	IDE_IOBUFFER_SIZE/IDE_SECTOR_SIZE

/* commands available in ata/atapi-6 rev3b */
#define IDE_CMD_CFA_ERASE_SECTOR			0xC0
#define IDE_CMD_CFA_REQUEST_EXTENDED_ERROR_CODE		0x03
#define IDE_CMD_CFA_TRANSLATE_SECTOR			0x87
#define IDE_CMD_CFA_WRITE_MULTIPLE_WITHOUT_ERASE	0xCD
#define IDE_CMD_CFA_WRITE_SECTORS_WITHOUT_ERASE		0x38
#define IDE_CMD_CHECK_MEDIA_CARD_TYPE			0xD1
#define IDE_CMD_CHECK_POWER_MODE			0xE5
#define IDE_CMD_DEVICE_CONFIGURATION			0xB1
#define IDE_CMD_DEVICE_RESET				0x08
#define IDE_CMD_DOWNLOAD_MICROCODE			0x92

#define IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC		0x90
#define IDE_CMD_FLUSH_CACHE				0xE7
#define IDE_CMD_FLUSH_CACHE_EXT				0xEA
#define IDE_CMD_GET_MEDIA_STATUS			0xDA
#define IDE_CMD_IDENTIFY_DEVICE				0xEC
#define IDE_CMD_IDENTIFY_PACKET_DEVICE			0xA1
#define IDE_CMD_IDLE					0xE3
#define IDE_CMD_IDLE_IMMEDIATE				0xE1
#define IDE_CMD_MEDIA_EJECT				0xED
#define IDE_CMD_MEDIA_LOCK				0xDE

#define IDE_CMD_MEDIA_UNLOCK				0xDF
#define IDE_CMD_NOP					0x00
#define IDE_CMD_PACKET					0xA0
#define IDE_CMD_READ_BUFFER				0xE4
#define IDE_CMD_READ_DMA				0xC8
#define IDE_CMD_READ_DMA_EXT				0x25
#define IDE_CMD_READ_DMA_QUEUED				0xC7
#define IDE_CMD_READ_DMA_QUEUED_EXT			0x26
#define IDE_CMD_READ_LOG_EXT				0x2F
#define IDE_CMD_READ_MULTIPLE				0xC4

#define IDE_CMD_READ_MULTIPLE_EXT			0x29
#define IDE_CMD_READ_NATIVE_MAX_ADDRESS			0xF8
#define IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT		0x27
#define IDE_CMD_READ_SECTORS				0x20
#define IDE_CMD_READ_SECTORS_EXT			0x24
#define IDE_CMD_READ_VERIFY_SECTORS			0x40
#define IDE_CMD_READ_VERIFY_SECTORS_EXT			0x42
#define IDE_CMD_SECURITY_DISABLE_PASSWORD		0xF6
#define IDE_CMD_SECURITY_ERASE_PREPARE			0xF3
#define IDE_CMD_SECURITY_ERASE_UNIT			0xF4

#define IDE_CMD_SECURITY_FREEZE_LOCK			0xF5
#define IDE_CMD_SECURITY_SET_PASSWORD			0xF1
#define IDE_CMD_SECURITY_UNLOCK				0xF2
#define IDE_CMD_SEEK					0x70
#define IDE_CMD_SERVICE					0xA2
#define IDE_CMD_SET_FEATURES				0xEF
#define IDE_CMD_SET_MAX					0xF9
#define IDE_CMD_SET_MAX_ADDRESS_EXT			0x37
#define IDE_CMD_SET_MULTIPLE_MODE			0xC6
#define IDE_CMD_SLEEP					0xE6

#define IDE_CMD_SMART					0xB0
#define IDE_CMD_STANDBY					0xE2
#define IDE_CMD_STANDBY_IMMEDIATE			0xE0
#define IDE_CMD_WRITE_BUFFER				0xE8
#define IDE_CMD_WRITE_DMA				0xCA
#define IDE_CMD_WRITE_DMA_EXT				0x35
#define IDE_CMD_WRITE_DMA_QUEUED			0xCC
#define IDE_CMD_WRITE_DMA_QUEUED_EXT			0x36
#define IDE_CMD_WRITE_LOG_EXT				0x3F
#define IDE_CMD_WRITE_MULTIPLE				0xC5

#define IDE_CMD_WRITE_MULTIPLE_EXT			0x39
#define IDE_CMD_WRITE_SECTORS				0x30
#define IDE_CMD_WRITE_SECTORS_EXT			0x34

// obsulete in ATA-6, but still used by some OSes
#define IDE_CMD_INITIALIZE_DEVICE_PARAMETERS		0x91
#define IDE_CMD_RECALIBRATE				0x10


#define IDE_CMD_DATA_NONE	0x00
#define IDE_CMD_DATA_OUT	0x01
#define IDE_CMD_DATA_IN		0x02
#define IDE_CMD_DMA_OUT		0x03
#define IDE_CMD_DMA_IN		0x04


class ide_channel_t;

class ide_device_t {
 public:
    IVMblock_handle_t *conhandle;

    /* ide io-port registers */
    union {
	u8_t raw;
	struct {
	    u8_t err : 1;
	    u8_t bit1: 1;
	    u8_t bit2: 1;
	    u8_t drq : 1;
	    u8_t dsc : 1;
	    u8_t df  : 1;
	    u8_t drdy: 1;
	    u8_t bsy : 1;
	} x;
    } reg_status;

    union {
	u8_t raw;
	struct {
	    u8_t bit0 : 1;
	    u8_t bit1 : 1;
	    u8_t abrt : 1;
	    u8_t : 1;
	    u8_t : 1;
	    u8_t : 1;
	    u8_t : 1;
	    u8_t bit7 : 1;

	} x;
    } reg_error;

    union {
	u8_t raw;
	struct {
	    u8_t bit0 : 1;
	    u8_t bit1 : 1;
	    u8_t bit2 : 1;
	    u8_t bit3 : 1;
	    u8_t dev  : 1;
	    u8_t bit5 : 1;
	    u8_t lba  : 1;
	    u8_t bit7 : 1;
	} x;
    } reg_device;

    union {
	u8_t raw;
	struct {
	    u8_t : 1;
	    u8_t nien : 1;
	    u8_t srst : 1;
	    u8_t reserved : 4;
	    u8_t hob : 1;
	} x;
    } reg_dev_control;


    // per device registers
    u8_t reg_feature;
    u8_t reg_lba_high;
    u8_t reg_lba_mid;
    u8_t reg_lba_low;
    u8_t reg_nsector;
    u8_t reg_alt_status;
    u16_t reg_data;

    // io buffer, 4K aligned for DMA transfer
    __attribute__((aligned(4096)))u8_t io_buffer[IDE_IOBUFFER_SIZE];
    // host physical address of io_buffer
    u32_t io_buffer_dma_addr;
    u32_t io_buffer_index;
    u32_t io_buffer_size;
    ide_channel_t *ch;

    u8_t np;
    u8_t serial[20];

    // device geometry
    u32_t lba_sector;
    u32_t sector;
    u32_t cylinder;
    u32_t head;

    u8_t data_transfer;
    u16_t req_sectors;

    // number of sectors per block used by read/write multiple
    u16_t mult_sectors;
    u8_t dev_num;
    u8_t udma_mode; // active udma_mode
    u8_t dma; // dma enabled

    ide_device_t() : np(1) {}

    u32_t get_sector() {
	return (reg_lba_low | (reg_lba_mid << 8) | (reg_lba_high << 16) | ((reg_device.raw & 0x0f) <<24)); }

    void set_sector(u32_t sec) {
	reg_lba_low = (u8_t)sec;
	reg_lba_mid = (u8_t)(sec>>8);
	reg_lba_high = (u8_t)(sec>>16);
	reg_device.raw = (reg_device.raw & 0xf0) | (sec>>24);
    }

};


//typedef struct ide_channel_s
class ide_channel_t {
 public:
    u8_t io_port;
    u8_t irq;

    ide_device_t master;
    ide_device_t slave;
    ide_device_t* cur_device;


};

typedef struct 
{
    volatile u32_t lock;
    u16_t cnt;
    volatile u16_t start_free;
    volatile u16_t start_dirty;
} L4VMblock_ring_t;


class ide_t {

 public:
    void init(void);
    void ide_portio( u16_t port, u32_t & value, bool read );
    void ide_irq_loop();
    void ide_start_dma(ide_device_t *);

 private:
    /* DD/OS specific data */
    L4_ThreadId_t serverid;
    IVMblock_handle_t handle;
    IVMblock_client_shared_t *client_shared;
    IVMblock_server_shared_t *server_shared;
    L4VMblock_ring_t ring_info;

    __attribute__((aligned(32768))) u8_t shared[32768];
    void l4vm_transfer_block( u32_t block, u32_t size, void *data, bool write, ide_device_t*,bool irq=true);

    /* IDE stuff */
    ide_channel_t channel[IDE_MAX_DEVICES/2];

    void ide_write_register( ide_channel_t *ch, u16_t reg, u16_t value );
    u16_t ide_read_register( ide_channel_t *ch, u16_t reg );
    u16_t ide_io_data_read( ide_device_t *dev );
    void ide_io_data_write( ide_device_t *dev, u16_t value );


    void ide_command( ide_device_t *dev, u16_t command );
    void ide_identify( ide_device_t * );
    void ide_software_reset( ide_channel_t *);
    void ide_raise_irq( ide_device_t * );
    void ide_read_sectors( ide_device_t * );
    void ide_write_sectors( ide_device_t * );
    void ide_read_dma( ide_device_t * );
    void ide_write_dma( ide_device_t * );
    void ide_set_features( ide_device_t * );

};

static const char *reg_to_str_read[] = {
    "data", "error",  "sector count", "lbalow",
    "lbamid", "lbahigh", "device","status",
    0,0,0,0,0,0, "alternate status"};


static const char *reg_to_str_write[] = { 
    "data", "feature", "sector count", "lbalow",
    "lbamid", "lbahigh", "device", "command",
    0,0,0,0,0,0, "device control" };

#endif /* !__DEVICE_IDE_H__ */

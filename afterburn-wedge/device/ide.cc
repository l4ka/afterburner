/*********************************************************************
 *
 * Copyright (C) 2007,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/ide.cc
 * Description:   Front-end emulation for IDE(ATA) disk/controller
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

/* Emulated harddisks are specified at command line with parameter
 * hd#=m,n
 * whereas # ranges from 0 to 3 and m,n specifies the major,minor number
 *
 * see http://www.lanana.org/docs/device-list/devices-2.6+.txt for a list
 * of available major/minor numbers
*/
// TODO this needs to be ported
#ifdef CONFIG_WEDGE_KAXEN
#include INC_ARCH(types.h)
void ide_portio( u16_t port, u32_t & value, bool read )
{
#if defined(CONFIG_DEVICE_IDE)
#error "Not ported!"
#endif
}
#else


#include <device/ide.h>
#if defined(CONFIG_DEVICE_I82371AB)
#include <device/i82371ab.h>
#endif
#include <console.h>
#include <L4VMblock_idl_client.h>
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(l4thread.h)
#include INC_ARCH(intlogic.h)
#include INC_ARCH(sync.h)
#include <string.h>

static unsigned char ide_irq_stack[KB(16)] ALIGNED(CONFIG_STACK_ALIGN);


static inline void ide_abort_command(ide_device_t *dev, u8_t error)
{
    dev->reg_status.raw = 0;
    dev->reg_status.x.err = 1;
    dev->reg_status.x.drdy = 1;
    dev->reg_alt_status = dev->reg_status.raw;
    dev->reg_error.raw = 0;
    dev->reg_error.x.abrt = 1;
}


static inline void ide_init_device(ide_device_t *dev)
{
    dev->reg_status.raw = 0;
    if(!dev->np)
	dev->reg_status.x.drdy = 1;
    dev->reg_alt_status = dev->reg_status.raw;
    dev->reg_error.raw = 0;

    dev->dma = 0;
    dev->udma_mode = 0;
}


// called on software reset
static inline void ide_set_signature(ide_device_t *dev)
{
    dev->reg_error.raw = 0x01;
    dev->reg_nsector = 0x01;
    dev->reg_lba_low = 0x01;
    dev->reg_lba_mid = 0x00;
    dev->reg_lba_high = 0x00;
    dev->reg_device.raw = 0x00;
}


static inline void ide_set_cmd_wait(ide_device_t *dev)
{
    dev->reg_status.raw = 0;
    dev->reg_status.x.drdy = 1;
    dev->reg_status.x.dsc = 1;
    dev->reg_error.raw = 0;
}


static inline void ide_set_data_wait( ide_device_t *dev )
{
    dev->reg_status.raw = 0;
    dev->reg_status.x.drq = 1;
    dev->reg_status.x.drdy = 1;
    dev->reg_status.x.dsc = 1;
    dev->reg_error.raw = 0;
}


// calculates C/H/S value from number of sectors
static void calculate_chs( u32_t sector, u32_t &cyl, u32_t &head, u32_t &sec )
{
    u32_t max;
    head = 16;
    sec = 63;
    max = sector/1008; // 16*63
    if(max > 16383)  // if more than 16,514,064 sectors, always return 16,383
	cyl = 16383;
    else
	cyl = max;
}


// from client26.c
static void
L4VMblock_deliver_server_irq( IVMblock_client_shared_t *client )
{

    L4_ThreadId_t from_tid;
    L4_MsgTag_t msg_tag;
    msg_tag.raw = 0; msg_tag.X.label = 0x100; msg_tag.X.u = 1;
    L4_Set_MsgTag( msg_tag );
    L4_LoadMR( 1, client->server_irq_no );
    L4_Set_MsgTag( msg_tag );
    L4_Ipc( client->server_irq_tid, L4_nilthread,
	    L4_Timeouts(L4_Never, L4_Never), &from_tid );
}


static void ide_acquire_lock(volatile u32_t *lock)
{
    u32_t old, val;
    return;
    do {
	old = *lock;
	val = cmpxchg(lock, old, (u32_t)0x1);
    } while (val != old);

}

static void ide_release_lock(volatile u32_t *lock)
{
    *lock = 0x0;
}


static void ide_irq_thread (void* params, l4thread_t *thread)
{
    ide_t *il = (ide_t*)params;
    il->ide_irq_loop();
    DEBUGGER_ENTER("IDE irq loop returned!");
}


void ide_t::ide_irq_loop()
{
    L4_ThreadId_t tid = L4_nilthread;
    volatile IVMblock_ring_descriptor_t *rdesc;

    for(;;) {
	L4_MsgTag_t tag = L4_Wait(&tid);
	ide_device_t *dev;
	dprintf(debug_ide_request, "Received virq from server\n");
	ide_acquire_lock(&ring_info.lock);
	client_shared->client_irq_pending=false;
	while( ring_info.start_dirty != ring_info.start_free ) {
	    // Get next transaction
	    rdesc = &client_shared->desc_ring[ ring_info.start_dirty ];
	    dprintf(debug_ide_request, "Checking transaction\n");
	    if( rdesc->status.X.server_owned) {
		dprintf(debug_ide_request, "Still server owned\n");
		break;
	    }
	    // TODO: signal error in ide part
	    if( rdesc->status.X.server_err ) {
		DEBUGGER_ENTER("error");
		} else {
		dev = (ide_device_t*)rdesc->client_data;
		ASSERT(dev);
		switch(dev->data_transfer) {

		case IDE_CMD_DATA_IN:
		    ide_set_data_wait(dev);
		    ide_raise_irq(dev);
		    break;

		case IDE_CMD_DATA_OUT:
		    if(dev->req_sectors>0)
			ide_set_data_wait(dev);
		    else
			ide_set_cmd_wait(dev);
		    ide_raise_irq(dev);
		    break;

#if defined(CONFIG_DEVICE_I82371AB)
		case IDE_CMD_DMA_IN:
		    {
		    ide_release_lock((u32_t*)&client_shared->dma_lock);
		    i82371ab_t *dma = i82371ab_t::get_device(dev->dev_num);
		    // check if bus master operation was started
		    if(!dma->get_ssbm(dev->dev_num)) {
			printf( "SSBM not set!\n");
			dma->set_dma_transfer_error(dev->dev_num);
		    }
		    else
			dma->set_dma_end(dev->dev_num);
		    ide_set_cmd_wait(dev);
		    ide_raise_irq(dev);
		    break;
		    }

		case IDE_CMD_DMA_OUT:
		    {
		    ide_release_lock((u32_t*)&client_shared->dma_lock);
		    i82371ab_t *dma = i82371ab_t::get_device(dev->dev_num);
		    // check if bus master operation was started
		    if(!dma->get_ssbm(dev->dev_num)) {
			printf( "SSBM not set!\n");
			dma->set_dma_transfer_error(dev->dev_num);
		    }
		    else
			dma->set_dma_end(dev->dev_num);
		    ide_set_cmd_wait(dev);
		    ide_raise_irq(dev);
		    break;
		    }
#endif
		default:
		    DEBUGGER_ENTER("Unhandled transfer type");
		}

	    }
	    // Move to next 
	    ring_info.start_dirty = (ring_info.start_dirty + 1) % ring_info.cnt;
	} /* while */
	ide_release_lock(&ring_info.lock);

    } /* for */
}


void ide_t::init(void)
{
    IVMblock_devid_t devid;
    CORBA_Environment ipc_env;
    IVMblock_devprobe_t probe_data;
    idl4_fpage_t client_mapping, server_mapping;
    L4_Fpage_t fpage;
    L4_Word_t shared_base = (L4_Word_t)&shared;
    L4_Word_t start, size;

    ipc_env = idl4_default_environment;

    printf( "Initializing IDE emulation\n");

    // locate VMblock server
    IResourcemon_query_interface( L4_Pager(), UUID_IVMblock, &serverid, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "unable to locate block server\n");
	return;
    }
    dprintf(debug_ide_ddos, "found blockserver with tid %t\n", serverid.raw);

    L4_ThreadId_t me = L4_Myself();
    IResourcemon_get_client_phys_range( L4_Pager(), &me, &start, &size, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free(&ipc_env);
	printf( "error resolving address");
	return;
    }

    dprintf(debug_ide_ddos, "PhysStart: %x size %08d\n", start, size);

    fpage = L4_Fpage(shared_base, 32768);

    dprintf(debug_ide_ddos, "Shared region at %x size %08d\n", L4_Address(fpage), L4_Size(fpage));

    // Register with the server
    idl4_set_rcv_window( &ipc_env, fpage);
    IVMblock_Control_register(serverid, &handle, &client_mapping, &server_mapping, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "unable to register with server\n");
	return;
    }

    dprintf(debug_ide_ddos, "Registered with server\n");
    dprintf(debug_ide_ddos, "\tclient mapping %x\n" ,idl4_fpage_get_base(client_mapping));
    dprintf(debug_ide_ddos, "\tserver mapping %x\n" ,idl4_fpage_get_base(server_mapping));

    client_shared = (IVMblock_client_shared_t*) (shared_base + idl4_fpage_get_base(client_mapping));
    server_shared = (IVMblock_server_shared_t*) (shared_base + idl4_fpage_get_base(server_mapping));

    // init stuff
    client_shared->client_irq_no = 15;
    ring_info.start_free = 0;
    ring_info.start_dirty = 0;
    ring_info.cnt = IVMblock_descriptor_ring_size;
    ring_info.lock = 0;
    ide_release_lock((u32_t*)&client_shared->dma_lock);

    dprintf(debug_ide_ddos, "IDE:\n\tServer: irq %d irq_tid %t main_tid %t\n\tClient:irq %d irq_tid %t main_tid %t\n",
		client_shared->server_irq_no, 
		(client_shared->server_irq_tid.raw),
		client_shared->server_main_tid.raw,
		client_shared->client_irq_no,
		(client_shared->client_irq_tid.raw),
		client_shared->client_main_tid.raw);
    dprintf(debug_ide_ddos, "\tphys offset  %x virt offset %x\n", 
		resourcemon_shared.wedge_phys_offset, resourcemon_shared.wedge_virt_offset);

    // Connected to server, now probe all devices and attach
    char devname[8];
    char *optname = "hd0=";
    char *next;
    for(int i=0 ; i < IDE_MAX_DEVICES ; i++) {
	int ch = i / 2;
	int sl = i % 2;
	ide_device_t *dev = (sl ? &(channel[ch].slave) : &(channel[ch].master) );

	dev->ch = &channel[ch];
	dev->np=1;

	*(optname+2) = '0' + i;

	if( !cmdline_key_search( optname, devname, 8) )
	    continue;

	devid.major = strtoul(devname, &next, 0);
	devid.minor = strtoul( strstr(devname, ",")+1, &next, 0);

	// some sanity checks
	if(devid.major > 258 || devid.minor > 255) {
	    printf( "Skipping suspicious device id !\n");
	    continue;
	}

	// save 'serial'
	strncpy((char*)dev->serial, devname, 20);
	for(int j=0;j<20;j++)
	    if(dev->serial[j] == ',')
		dev->serial[j] = '0';

	dprintf(debug_ide_ddos, "Probing hd %d with major %d minor %d\n",
		i, devid.major, devid.minor);
	
	IVMblock_Control_probe( serverid, &devid, &probe_data, &ipc_env );
	if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	    CORBA_exception_free(&ipc_env);
	    printf( "error probing device\n");
	    continue;
	}
	
	if( probe_data.hardsect_size != 512 ) {
	    printf( "Unusual sector size %d\n", probe_data.hardsect_size);
	    continue;
	}
	
	dprintf(debug_ide_ddos, "block size %d hardsect size %d sectors %d\n",
		probe_data.block_size, probe_data.hardsect_size,
		probe_data.device_size);
	
	dev->lba_sector = probe_data.device_size;

	// Attach to device
	IVMblock_Control_attach( serverid, handle, &devid, 3, dev->conhandle, &ipc_env);
	if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	    CORBA_exception_free(&ipc_env);
	    printf( "error attaching to device\n");
	    continue;
	}

	// calculate physical address
	dev->io_buffer_dma_addr = (u32_t)&dev->io_buffer - resourcemon_shared.wedge_virt_offset + resourcemon_shared.wedge_phys_offset;
	dprintf(debug_ide_ddos, "IO buffer at %x\n" ,dev->io_buffer_dma_addr);

	dev->np=0;
	dev->dev_num = i;
	ide_set_signature( dev );
	ide_init_device( dev );

	calculate_chs(dev->lba_sector, dev->cylinder, dev->head, dev->sector);
	printf( "IDE hd %d %d sectors (C/H/S: %d/%d/%d)\n", i, dev->lba_sector, dev->cylinder,
		dev->head, dev->sector);
	}

    for(int i=0; i < (IDE_MAX_DEVICES/2) ; i++) {
	channel[i].cur_device = &channel[i].master;
	channel[i].irq = (i ? 15 : 14);
    }

   
#if defined(CONFIG_DEVICE_PASSTHRU)
    intlogic_t &intlogic = get_intlogic();
    intlogic.add_hwirq_squash(14);
    intlogic.add_hwirq_squash(15);
#endif
    //dbg_irq(14);
    //dbg_irq(15);

    // start irq loop thread
    vcpu_t &vcpu = get_vcpu();
    l4thread_t *irq_thread = 
	get_l4thread_manager()->create_thread( &vcpu, (L4_Word_t)ide_irq_stack,
	     sizeof(ide_irq_stack), resourcemon_shared.prio, ide_irq_thread, 
					      L4_Pager(), this );
    if( !irq_thread ) {
	printf( "Error creating ide irq thread\n");
	DEBUGGER_ENTER("irq thread");
	return;
    }
    client_shared->client_irq_tid = irq_thread->get_global_tid();
    client_shared->client_main_tid = irq_thread->get_global_tid();
    irq_thread->start();
    printf( "IDE IRQ loop started with thread id %t\n", irq_thread->get_global_tid().raw);
}


void ide_t::ide_write_register(ide_channel_t *ch, u16_t reg, u16_t value)
{
    dprintf(debug_ide, "IDE write reg %C val %x\n", 
	    DEBUG_TO_4CHAR(reg_to_str_write[reg]), value);
    switch(reg) {
    case 0: 
	ide_io_data_write( ch->cur_device, value);
	break;
    case 1:
	ch->master.reg_feature = value;
	ch->slave.reg_feature = value;
	break;
    case 2:
	ch->master.reg_nsector = value;
	ch->slave.reg_nsector = value;
	break;
    case 3:
	ch->master.reg_lba_low = value;
	ch->slave.reg_lba_low = value;
	break;
    case 4:
	ch->master.reg_lba_mid = value;
	ch->slave.reg_lba_mid = value;
	break;
    case 5:
	ch->master.reg_lba_high = value;
	ch->slave.reg_lba_high = value;
	break;
    case 6:
	ch->master.reg_device.raw = value;
	ch->slave.reg_device.raw = value;
	if(value & 0x10)
	    ch->cur_device = &ch->slave;
	else
	    ch->cur_device = &ch->master;
	break;
    case 7:
	ide_command( ch->cur_device, value );
	break;
    case 14:
	// srst set to one, set busy
	if( !ch->master.reg_dev_control.x.srst && (value & 0x4)) {
	    ch->master.reg_status.x.bsy = 1;
	    ch->slave.reg_status.x.bsy = 1;
	} 
	// srst cleared to zero
	else if( ch->master.reg_dev_control.x.srst && !(value & 0x4)) {
	    ch->master.reg_status.x.bsy = 0;
	    ch->slave.reg_status.x.bsy = 0;
	    ide_software_reset(ch);
	}
	ch->master.reg_dev_control.raw = value;
	ch->slave.reg_dev_control.raw = value;
	break;
    default:
	printf( "IDE write to unknown register %d\n", reg);
    }
}


u16_t ide_t::ide_read_register(ide_channel_t *ch, u16_t reg)
{
    ide_device_t *dev = ch->cur_device;
    u16_t val;
    
    if(dev->np)
	return 0;

    switch(reg) {
    case 0: 
	val = ide_io_data_read(dev);
	break;
    case 1: 
	val = dev->reg_error.raw;
	break;
    case 2: 
	val = dev->reg_nsector;
	break;
    case 3: 
	val = dev->reg_lba_low;
	break;
    case 4: 
	val = dev->reg_lba_mid;
	break;
    case 5: 
	val = dev->reg_lba_high;
	break;
    case 6: 
	val = dev->reg_device.raw;
	break;
    case 7: 
	val = dev->reg_status.raw;
	//    case 14: val = dev->reg_alt_status;
	break;
    case 14: 
	val = dev->reg_status.raw;
	break;
    default:	
	printf( "IDE read from unknown register %d\n", reg);
	val = 0;
	
    }
    //dprintf(debug_ide, "IDE read reg %C val %x\n", 
    //    DEBUG_TO_4CHAR(reg_to_str_read[reg]), val);

    return val;
}


void ide_t::ide_raise_irq( ide_device_t *dev )
{
    if( !(dev->reg_dev_control.x.nien))
    {
	dprintf(debug_ide, "IDE raise irq %d\n", dev->ch->irq);
	get_intlogic().raise_irq(dev->ch->irq);
    }
}


void ide_t::ide_portio( u16_t port, u32_t & value, bool read )
{
    u16_t reg = port & 0xF;// + ((port & 0x200)>>8) 

    if(read)
	switch( port &~(0xf)) {
	case 0x1F0:
	    value = ide_read_register( &channel[0], reg);
	    break;
	case 0x3F0:
	    value = ide_read_register( &channel[0], reg+8);
	    break;
	case 0x170:
	    value = ide_read_register( &channel[1], reg);
	    break;
	case 0x370:
	    value = ide_read_register( &channel[1], reg+8);
	    break;
	default:
	    printf( "IDE read from unknown port %d\n", port);

	}
    else
	switch(port & ~(0xf)) {
	case 0x1F0:
	    ide_write_register( &channel[0], reg, value );
	    break;
	case 0x3F0:
	    ide_write_register( &channel[0], reg+8, value );
	    break;
	case 0x170:
	    ide_write_register( &channel[1], reg, value );
	    break;
	case 0x370:
	    ide_write_register( &channel[1], reg+8, value );
	    break;
	default:
	    printf( "IDE write to unknown port %d\n", port);
	}
}


u16_t ide_t::ide_io_data_read( ide_device_t *dev )
{
    if(dev->data_transfer != IDE_CMD_DATA_IN) {
	printf( "IDE read with no data request\n");
	return 0xff;
    }
    u16_t dat = *((u16_t*)(dev->io_buffer+dev->io_buffer_index));
    dev->io_buffer_index += 2;

    if(dev->io_buffer_index >= dev->io_buffer_size) { // transfer complete
	if( dev->req_sectors > 0) {
	    u32_t n = dev->req_sectors;
	    if( n > IDE_IOBUFFER_BLOCKS)
		n = IDE_IOBUFFER_BLOCKS;
	    l4vm_transfer_block( dev->get_sector(), n*IDE_SECTOR_SIZE, (void*)dev->io_buffer_dma_addr, false, dev);
	    dev->req_sectors -= n;
	    dev->io_buffer_index = 0;
	    dev->io_buffer_size = n * IDE_SECTOR_SIZE;
	    dev->set_sector( dev->get_sector() + n);
	    dev->reg_status.x.bsy = 1;
	    }
	else {
	    dev->data_transfer = IDE_CMD_DATA_NONE;
	    ide_set_cmd_wait(dev);
	}
	return dat;
    }
    // TODO: check if raising the INT here is too early
    if( (dev->io_buffer_index % IDE_SECTOR_SIZE)==0) // next DRQ data block
	ide_raise_irq(dev);
    return dat;
}


void ide_t::ide_io_data_write( ide_device_t *dev, u16_t value )
{
    if(dev->data_transfer != IDE_CMD_DATA_OUT) { // do nothing
	printf( "IDE write with no data request\n");
	return;
    }
    //    printf( "IDE write value " <<(void*)value << '\n';
    *((u16_t*)(dev->io_buffer+dev->io_buffer_index)) = value;
    dev->io_buffer_index += 2;

    if(dev->io_buffer_index >= dev->io_buffer_size) {
	if( dev->req_sectors > 0 ) {
	    u32_t n = dev->req_sectors;
	    if( n > IDE_IOBUFFER_BLOCKS )
		n = IDE_IOBUFFER_BLOCKS;
	    l4vm_transfer_block( dev->get_sector(), n*IDE_SECTOR_SIZE, (void*)dev->io_buffer_dma_addr, true, dev);
	    dev->req_sectors -= n;
	    dev->io_buffer_index = 0;
	    dev->io_buffer_size = n * IDE_SECTOR_SIZE;
	    dev->set_sector( dev->get_sector() + n );
	    dev->reg_status.x.bsy = 1;
	}
	else {
	    dev->reg_status.x.bsy = 1;
	    l4vm_transfer_block( dev->get_sector(), dev->io_buffer_size, (void*)dev->io_buffer_dma_addr, true, dev);
	}
	return;
    }

    // TODO: see above
    if( (dev->io_buffer_index % IDE_SECTOR_SIZE)==0) // next DRQ data block
	ide_raise_irq(dev);
}


void ide_t::ide_command(ide_device_t *dev, u16_t cmd)
{
    // ignore commands to non existent devices
    if(dev->np) {
	printf( "Ignoring command to np device\n");
	return;
    }

    dprintf(debug_ide, "IDE command %x\n", cmd);

    switch(cmd) {
	
    case IDE_CMD_CFA_ERASE_SECTOR:
    case IDE_CMD_CFA_REQUEST_EXTENDED_ERROR_CODE:
    case IDE_CMD_CFA_TRANSLATE_SECTOR:
    case IDE_CMD_CFA_WRITE_MULTIPLE_WITHOUT_ERASE:
    case IDE_CMD_CFA_WRITE_SECTORS_WITHOUT_ERASE:
    case IDE_CMD_CHECK_MEDIA_CARD_TYPE:
    case IDE_CMD_CHECK_POWER_MODE:
    case IDE_CMD_DEVICE_CONFIGURATION:
    case IDE_CMD_DEVICE_RESET:
    case IDE_CMD_DOWNLOAD_MICROCODE:

    case IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC:
    case IDE_CMD_FLUSH_CACHE:
    case IDE_CMD_FLUSH_CACHE_EXT:
    case IDE_CMD_GET_MEDIA_STATUS:
	break;

    case IDE_CMD_IDENTIFY_DEVICE:
	ide_identify(dev); 
	ide_raise_irq(dev);
	return;

    case IDE_CMD_IDENTIFY_PACKET_DEVICE:
    case IDE_CMD_IDLE:
    case IDE_CMD_IDLE_IMMEDIATE:
    case IDE_CMD_MEDIA_EJECT:
    case IDE_CMD_MEDIA_LOCK:

    case IDE_CMD_MEDIA_UNLOCK:
    case IDE_CMD_NOP:
    case IDE_CMD_PACKET:
    case IDE_CMD_READ_BUFFER:
	break;
    case IDE_CMD_READ_DMA:
	ide_read_dma(dev);
	return;
    case IDE_CMD_READ_DMA_EXT:
    case IDE_CMD_READ_DMA_QUEUED:

    case IDE_CMD_READ_DMA_QUEUED_EXT:
    case IDE_CMD_READ_LOG_EXT:
    case IDE_CMD_READ_MULTIPLE:

    case IDE_CMD_READ_MULTIPLE_EXT:
    case IDE_CMD_READ_NATIVE_MAX_ADDRESS:
    case IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT:
	break;
    case IDE_CMD_READ_SECTORS:
	ide_read_sectors(dev);
	return;
    case IDE_CMD_READ_SECTORS_EXT:
    case IDE_CMD_READ_VERIFY_SECTORS:
    case IDE_CMD_READ_VERIFY_SECTORS_EXT:
    case IDE_CMD_SECURITY_DISABLE_PASSWORD:
    case IDE_CMD_SECURITY_ERASE_PREPARE:
    case IDE_CMD_SECURITY_ERASE_UNIT:

    case IDE_CMD_SECURITY_FREEZE_LOCK:
    case IDE_CMD_SECURITY_SET_PASSWORD:
    case IDE_CMD_SECURITY_UNLOCK:
    case IDE_CMD_SEEK:
    case IDE_CMD_SERVICE:
	break;

    case IDE_CMD_SET_FEATURES:
	ide_set_features(dev);
	ide_raise_irq(dev);
	return;

    case IDE_CMD_SET_MAX:
    case IDE_CMD_SET_MAX_ADDRESS_EXT:
	break;

    case IDE_CMD_SET_MULTIPLE_MODE:
	// TODO: check for power of 2
	if(dev->reg_nsector > IDE_MAX_READ_WRITE_MULTIPLE)
	    ide_abort_command(dev, 0);
	else {
	    dev->mult_sectors = dev->reg_nsector;
	    ide_set_cmd_wait(dev);
	}
	ide_raise_irq(dev);
	return;

    case IDE_CMD_SLEEP:

    case IDE_CMD_SMART:
    case IDE_CMD_STANDBY:
    case IDE_CMD_STANDBY_IMMEDIATE:
    case IDE_CMD_WRITE_BUFFER:
	break;
    case IDE_CMD_WRITE_DMA:
	ide_write_dma(dev);
	return;
    case IDE_CMD_WRITE_DMA_EXT:
    case IDE_CMD_WRITE_DMA_QUEUED:
    case IDE_CMD_WRITE_DMA_QUEUED_EXT:
    case IDE_CMD_WRITE_LOG_EXT:
    case IDE_CMD_WRITE_MULTIPLE:

    case IDE_CMD_WRITE_MULTIPLE_EXT:
	break;
    case IDE_CMD_WRITE_SECTORS:
	ide_write_sectors(dev);
	return;
    case IDE_CMD_WRITE_SECTORS_EXT:
	break;

	// linux keeps calling this old stuff, simply do nothing
    case IDE_CMD_INITIALIZE_DEVICE_PARAMETERS:
    case IDE_CMD_RECALIBRATE:
	ide_set_cmd_wait(dev);
	ide_raise_irq(dev);
	return;

    default:
	printf( "IDE unknown command %d\n", cmd);
	ide_abort_command(dev, 0);
	ide_raise_irq(dev);
	return;
    }
    ide_abort_command(dev, 0);
    ide_raise_irq(dev);
}


// from qemu hw/ide.c
static void padstr(char *str, const char *src, u32_t len)
{
    u32_t i, v;
    for(i = 0; i < len; i++) {
        if (*src)
            v = *src++;
        else
            v = ' ';
        *(char *)((long)str ^ 1) = v;
        str++;
    }
}


/* 8.15, p. 113 */
void ide_t::ide_identify( ide_device_t *dev )
{
    dprintf(debug_ide, "IDE identify command\n");

    memset(dev->io_buffer, 0, 512);
    u16_t *buf = (u16_t*)dev->io_buffer;

    // fill buffer with identify information
    *(buf) = 0x0000;
    // obsolete since ata-6, but most OSes use it nevertheless
    *(buf+1) = dev->cylinder;
    *(buf+3) = dev->head;
    *(buf+6) = dev->sector;

    padstr( (char*)(buf+10), (char*)dev->serial, 20 );
    padstr( (char*)(buf+23), "l4ka 0.1", 8 );
    padstr( (char*)(buf+27), "L4KA AFTERBURNER HARDDISK", 40 );
    *(buf+47) = 0x8000 | IDE_MAX_READ_WRITE_MULTIPLE;
    *(buf+49) = 0x0300;
#if defined(CONFIG_DEVICE_I82371AB)
    *(buf+53) = 6;
#else
    *(buf+53) = 2;
#endif
    *(buf+59) = 0x0100 | 1 ;
    *(buf+60) = dev->lba_sector;
    *(buf+61) = dev->lba_sector >> 16;
    *(buf+64) = 3;	// pio mode 3 and 4
    *(buf+67) = 120;
    *(buf+68) = 120;
    *(buf+80) = 0x0070; // supports ata-6
    *(buf+81) = 0x0019; // ata-6 rev3a
    *(buf+82) = 0x4200; // support device reset and nop command
    *(buf+83) = (1 << 14);
    *(buf+84) = (1 << 14);
    *(buf+85) = 0;
    *(buf+86) = 0;
    *(buf+87) = (1 << 14);
    if(dev->dma)
	*(buf+88) = 7 | (1 << (dev->udma_mode + 8)); // udma support
    else
	*(buf+88) = 7; 
    *(buf+93) = (1 << 14) | 1;
    *(buf+100) = 0; // lba48

    ide_set_data_wait(dev);

    // setup data transfer
    dev->io_buffer_index = 0;
    dev->io_buffer_size = IDE_SECTOR_SIZE;
    dev->data_transfer = IDE_CMD_DATA_IN;
}



// 9.2, p. 315
void ide_t::ide_software_reset( ide_channel_t *ch)
{
    if(!ch->master.np) {
	if(ch->slave.np)
	    ch->master.reg_error.x.bit7 = 0;
	else
	    ch->master.reg_device.x.dev = 0;
	ch->master.reg_status.raw &= ~(0xD);
	ide_set_signature(&ch->master);
    }
    if(!ch->slave.np) {
	ch->slave.reg_device.x.dev = 0;
	ch->slave.reg_status.raw &= ~(0xD);
	ide_set_signature(&ch->slave);
    }
}


/* client code for the dd/os block server */
void ide_t::l4vm_transfer_block( u32_t block, u32_t size, void *data, bool write, ide_device_t* dev )
{
    volatile IVMblock_ring_descriptor_t *rdesc;

    // get next free ring descriptor
    ide_acquire_lock(&ring_info.lock);
    rdesc = &client_shared->desc_ring[ ring_info.start_free ];
    ASSERT( !rdesc->status.X.server_owned );
    ring_info.start_free = (ring_info.start_free+1) % ring_info.cnt;
    ide_release_lock(&ring_info.lock);

    rdesc->handle = handle;
    rdesc->size = size;
    rdesc->offset = block;
    rdesc->page = (void**)(data);
    rdesc->client_data = (void**)dev;
    rdesc->status.raw = 0;
    rdesc->status.X.do_write = write;
    rdesc->status.X.speculative = 0;
    rdesc->status.X.server_owned = 1;

    server_shared->irq_status |= 1; // was L4VMBLOCK_SERVER_IRQ_DISPATCH
    server_shared->irq_pending = true;
    dprintf(debug_ide_request, "Sending request for block %d size %d (%c)\n", block, size, (write ? 'W' : 'R'));
    L4VMblock_deliver_server_irq(client_shared);
}


/* additional dma client code for dd/os
 * Note: In the context of DMA controllers, write transfers move data from a device
 * to memory whereas here write transfers data from memory to a device.
 */
void ide_t::l4vm_transfer_dma( u32_t block, ide_device_t *dev, void *dsct , bool write)
{
#if defined(CONFIG_DEVICE_I82371AB)
    int n;
    volatile IVMblock_ring_descriptor_t *rdesc;
    prdt_entry_t *pe = (prdt_entry_t*)dsct;

    // get next free ring descriptor
    ide_acquire_lock(&ring_info.lock);
    rdesc = &client_shared->desc_ring[ ring_info.start_free ];
    ASSERT( !rdesc->status.X.server_owned );
    ring_info.start_free = (ring_info.start_free+1) % ring_info.cnt;
    ide_release_lock(&ring_info.lock);

    rdesc->size = 0;
    rdesc->handle = handle;
    rdesc->offset = block;

    // get lock for dma descriptor table
    ide_acquire_lock((u32_t*)&client_shared->dma_lock);
    // copy descriptor table entries
    for( n=0 ; n < 512 ; n++) {
	client_shared->dma_vec[n].size = pe->transfer.fields.count;
	client_shared->dma_vec[n].page = (void**)pe->base_addr;
	if(pe->transfer.fields.eot)
	    break;
	pe++;
    }
    if( n >= IVMblock_descriptor_max_vectors ) {
	pe = (prdt_entry_t*)dsct;
	for( n=0 ; n < 512 ; n++) {
	    printf( "Transfer %d bytes to %x\n", pe->transfer.fields.count, pe->base_addr);
	    if(pe->transfer.fields.eot)
		break;
	    pe++;
	}
	DEBUGGER_ENTER("ohje");
    }

    rdesc->count = n+1;
    rdesc->client_data = (void**)dev;
    rdesc->status.raw = 0;
    rdesc->status.X.do_write = write;
    rdesc->status.X.speculative = 0;
    rdesc->status.X.server_owned = 1;

    server_shared->irq_status |= 1; // was L4VMBLOCK_SERVER_IRQ_DISPATCH
    server_shared->irq_pending = true;
    dprintf(debug_ide_request, "Sending request for block %d with %d entries (%c)\n",
	    block, n+1, ( write ? 'W' : 'R'));
    L4VMblock_deliver_server_irq(client_shared);
#endif
}


// 8.34, p. 199
void ide_t::ide_read_sectors( ide_device_t *dev )
{
    u32_t sector = 0, n;

    n = dev->req_sectors = ( dev->reg_nsector ? dev->reg_nsector : 256 );

    if( dev->reg_device.x.lba )
	sector = dev->get_sector();
    else {
	printf( "IDE read chs access\n");
	DEBUGGER_ENTER("TODO");
    }

    if( n > IDE_IOBUFFER_BLOCKS)
	n = IDE_IOBUFFER_BLOCKS;
    l4vm_transfer_block( sector, n*IDE_SECTOR_SIZE, (void*)dev->io_buffer_dma_addr, false, dev);
    dev->data_transfer = IDE_CMD_DATA_IN;
    dev->io_buffer_index = 0;
    dev->io_buffer_size = IDE_SECTOR_SIZE * n ;
    dev->req_sectors -= n;
    dev->set_sector( sector + n );
    dev->reg_status.x.bsy = 1;
}


// 8.62, p. 303
void ide_t::ide_write_sectors( ide_device_t *dev )
{
    u32_t sector, n;

    n = dev->req_sectors = ( dev->reg_nsector ? dev->reg_nsector : 256 );

    if( dev->reg_device.x.lba ) {
	sector = dev->get_sector();
    }
    else {
	printf( "IDE write chs access\n");
	DEBUGGER_ENTER("TODO");
    }


    if( n > IDE_IOBUFFER_BLOCKS )
	n = IDE_IOBUFFER_BLOCKS;

    dev->data_transfer = IDE_CMD_DATA_OUT;
    dev->io_buffer_size = IDE_SECTOR_SIZE * n;
    dev->io_buffer_index = 0;
    dev->req_sectors -= n;

    ide_set_data_wait(dev);
    dev->reg_status.x.drdy=1;
    ide_raise_irq(dev);
}


// Called from i82371ab when guest enables/starts dma bus master operation
void ide_t::ide_start_dma( ide_device_t *dev, bool write)
{
#if defined(CONFIG_DEVICE_I82371AB)
    u32_t sector, size = 0;

    i82371ab_t *dma = i82371ab_t::get_device(dev->dev_num);
    prdt_entry_t *pe = dma->get_prdt_entry(dev->dev_num);

    // check prd entries
    for(int i=0;i<512;i++) {
	size += pe->transfer.fields.count;
	if(pe->transfer.fields.eot)
	    break;
	pe++;
    }

    if( (u32_t) (dev->req_sectors*IDE_SECTOR_SIZE) > size) { // prd has smaller buffer size
	dma->set_dma_transfer_error(dev->dev_num);
	ide_raise_irq(dev);
	printf( "prd too small\n");
	printf( "size %d request %d\n", size, (dev->req_sectors*IDE_SECTOR_SIZE));
	return;
    }
    if( !dev->reg_device.x.lba ) {
	printf( "IDE DMA chs access\n");
	DEBUGGER_ENTER("TODO");
    }
    sector = dev->get_sector();

    l4vm_transfer_dma( sector, dev, (void*)dma->get_prdt_entry(dev->dev_num), write );

#endif
}


// 8.25, p. 166
void ide_t::ide_read_dma( ide_device_t *dev )
{
#if defined(CONFIG_DEVICE_I82371AB)
    
    if(!dev->dma) { // dma disabled
	ide_abort_command(dev,0);
	ide_raise_irq(dev);
	return;
    }

    dev->req_sectors = ( dev->reg_nsector ? dev->reg_nsector : 256 );
    //    printf( n << " sectors\n");
    i82371ab_t::get_device(dev->dev_num)->register_device(dev->dev_num, dev);

    dev->data_transfer = IDE_CMD_DMA_IN;
    ide_set_data_wait(dev);

#else 
    // no dma support, abort
    ide_abort_command(dev, 0);
    ide_raise_irq(dev);
#endif
}


void ide_t::ide_write_dma( ide_device_t *dev )
{
#if defined(CONFIG_DEVICE_I82371AB)
    if(!dev->dma) { // dma disabled
	ide_abort_command(dev,0);
	ide_raise_irq(dev);
	return;
    }

    dev->req_sectors = ( dev->reg_nsector ? dev->reg_nsector : 256 );

    i82371ab_t::get_device(dev->dev_num)->register_device(dev->dev_num, dev);

    dev->data_transfer = IDE_CMD_DMA_OUT;
    ide_set_data_wait(dev);

#else 
    // no dma support, abort
    ide_abort_command(dev, 0);
    ide_raise_irq(dev);
#endif
}


// 8.46, p. 224
void ide_t::ide_set_features( ide_device_t *dev )
{
    switch(dev->reg_feature) 
	{
	case 0x03 : // set transfer mode
	    switch(dev->reg_nsector >> 3) 
		{
		case 0x0: // pio default mode
		    dev->dma=0;
		    break;

		case 0x8: // udma mode
		    dev->dma=1;
		    dev->udma_mode = dev->reg_nsector & 0x7;
		    break;

		default:
		    ide_abort_command(dev, 0);
		    return;
		}
	    break;

	    
	default:
	    ide_abort_command(dev, 0);
	    return;
	}
    ide_set_cmd_wait(dev);
}


void ide_portio( u16_t port, u32_t & value, bool read )
{
#if defined(CONFIG_DEVICE_IDE)
extern ide_t ide;
    ide.ide_portio( port, value, read);
#endif
}
#endif


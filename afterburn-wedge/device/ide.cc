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

#define GENHD_FL_REMOVABLE			1
#define GENHD_FL_DRIVERFS			2
#define GENHD_FL_CD				8
#define GENHD_FL_UP				16
#define GENHD_FL_SUPPRESS_PARTITION_INFO	32

#include <device/ide.h>
#if defined(CONFIG_DEVICE_I82371AB)
#include <device/i82371ab.h>
#endif
#include <console.h>
#include <L4VMblock_idl_client.h>
#include INC_WEDGE(backend.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(message.h)
#include INC_ARCH(intlogic.h)
#include INC_ARCH(sync.h)
#include <string.h>

static unsigned char ide_irq_stack[KB(16)] ALIGNED(CONFIG_STACK_ALIGN);


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
    dprintf(debug_ide, "found blockserver with tid %t\n", serverid.raw);

    L4_ThreadId_t me = L4_Myself();
    IResourcemon_get_client_phys_range( L4_Pager(), &me, &start, &size, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free(&ipc_env);
	printf( "error resolving address");
	return;
    }

    dprintf(debug_ide, "PhysStart: %x size %08d\n", start, size);

    fpage = L4_Fpage(shared_base, 0x8000);

    dprintf(debug_ide, "Shared region at %x size %08d\n", L4_Address(fpage), L4_Size(fpage));

    // Register with the server
    idl4_set_rcv_window( &ipc_env, fpage);
    IVMblock_Control_register(serverid, &handle, &client_mapping, &server_mapping, &ipc_env);
    if( ipc_env._major != CORBA_NO_EXCEPTION) {
	printf( "unable to register with server\n");
	return;
    }

    client_shared = (IVMblock_client_shared_t*) (shared_base + idl4_fpage_get_base(client_mapping));
    server_shared = (IVMblock_server_shared_t*) (shared_base + idl4_fpage_get_base(server_mapping));

    dprintf(debug_ide, "Registered with server\n");
    dprintf(debug_ide, "\tclient mapping %x\n", client_shared);
    dprintf(debug_ide, "\tserver mapping %x\n", client_shared);

    // init stuff
    client_shared->client_irq_no = 15;
    
    ring_info.start_free = 0;
    ring_info.start_dirty = 0;
    ring_info.cnt = IVMblock_descriptor_ring_size;
    ring_info.lock = 0;
    ide_release_lock((u32_t*)&client_shared->dma_lock);

    dprintf(debug_ide, "IDE:\n\tServer: irq %d irq_tid %t main_tid %t\n\tClient:irq %d irq_tid %t main_tid %t\n",
	    client_shared->server_irq_no, client_shared->server_irq_tid,  client_shared->server_main_tid,
	    client_shared->client_irq_no, client_shared->client_irq_tid, client_shared->client_main_tid.raw);
    dprintf(debug_ide, "\tphys offset  %x virt offset %x\n", 
	    resourcemon_shared.wedge_phys_offset, resourcemon_shared.wedge_virt_offset);

    // Connected to server, now probe all devices and attach
    char devname[8];
    char *optname = "hd0=";
    char *next;
    
    for(int i=0 ; i < IDE_MAX_DEVICES ; i++) 
    {	
	int ch = i / 2;
	ide_device_t *dev = get_device(i);
	
	dev->ch = &channel[ch];
	dev->ide = this;
	dev->dev_num = i;
	dev->init_device();
	dev->set_signature();
	
	// calculate physical address
	dev->io_buffer_dma_addr = (u32_t)&dev->io_buffer 
	    - resourcemon_shared.wedge_virt_offset + 
	    resourcemon_shared.wedge_phys_offset;
	dprintf(debug_ide, "\tio buffer at %x (gphys %x)\n", dev->io_buffer_dma_addr, dev->io_buffer);


	*(optname+2) = '0' + i;
	
	if( !cmdline_key_search( optname, devname, 8) )
	{
	    printf("IDE %d (N/A)\n",  i);	
	    continue;
	}
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

	dprintf(debug_ide, "Probing IDE dev %d with major %d minor %d\n",
		i, devid.major, devid.minor);
	
	IVMblock_Control_probe( serverid, &devid, &probe_data, &ipc_env );
	
	if( ipc_env._major != CORBA_NO_EXCEPTION ) 
	{
	    printf("IDE %d (N/A)\n",  i);	
	    CORBA_exception_free(&ipc_env);
	    continue;
	}
	
	printf("IDE %d (%C), block size %d hardsect size %d sectors %d\n",
	       i, ((probe_data.device_flags & GENHD_FL_CD) ? 
		   DEBUG_TO_4CHAR("CD-ROM") : DEBUG_TO_4CHAR("HDD")), 
	       probe_data.block_size, probe_data.hardsect_size,
	       probe_data.device_size);
	
	// Attach to device
	IVMblock_Control_attach( serverid, handle, &devid, 3, dev->conhandle, &ipc_env);
	if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	    CORBA_exception_free(&ipc_env);
	    printf( "error attaching to device\n");
	    continue;
	}
	
	dev->lba_sector = probe_data.device_size;
	dev->calculate_chs();
	
	printf( "\t%d sectors (C/H/S: %d/%d/%d)\n", dev->lba_sector, dev->chs.cylinder, 
		dev->chs.head, dev->chs.sector);

	// calculate physical address
	dev->io_buffer_dma_addr = (u32_t)&dev->io_buffer 
	    - resourcemon_shared.wedge_virt_offset + 
	    resourcemon_shared.wedge_phys_offset;
	dprintf(debug_ide, "\tio buffer at %x (gphys %x)\n", dev->io_buffer_dma_addr, dev->io_buffer);

	dev->cdrom = (probe_data.device_flags & GENHD_FL_CD);
	dev->present = true;
	dev->reg_status.x.drdy = 1;
	
	
    }

    for(int i=0; i < (IDE_MAX_DEVICES/2) ; i++) {
	channel[i].chnum = i;
	channel[i].current = 0;
	channel[i].irq = (i ? 15 : 14);
    }

   
    intlogic_t &intlogic = get_intlogic();
    
    intlogic.add_virtual_hwirq(14);
    intlogic.clear_hwirq_squash(14);
    intlogic.add_virtual_hwirq(15);
    intlogic.clear_hwirq_squash(15);
    
    //dbg_irq(14);
    //dbg_irq(15);

    // start irq loop thread
    vcpu_t &vcpu = get_vcpu();
    l4thread_t *irq_thread = get_l4thread_manager()->create_thread( &vcpu, (L4_Word_t)ide_irq_stack,
								    sizeof(ide_irq_stack), 
								    resourcemon_shared.prio, 
								    ide_irq_thread, 
								    L4_Pager(), 
								    this );
    if( !irq_thread ) {
	printf( "Error creating IDE irq thread\n");
	DEBUGGER_ENTER("irq thread");
	return;
    }
    client_shared->client_irq_tid = irq_thread->get_global_tid();
    client_shared->client_main_tid = irq_thread->get_global_tid();
    irq_thread->start();
    dprintf(debug_ide, "IDE IRQ loop started with thread id %t\n", irq_thread->get_global_tid().raw);
}

void ide_t::ide_irq_loop()
{
    L4_ThreadId_t tid = L4_nilthread;
    volatile IVMblock_ring_descriptor_t *rdesc;

    for(;;) {
	L4_MsgTag_t tag = L4_Wait(&tid);
	
	ide_device_t *dev;
	
	dprintf(debug_ide, "IDE virq from server\n");
	
	ide_acquire_lock(&ring_info.lock);
	client_shared->client_irq_pending=false;
	
	while( ring_info.start_dirty != ring_info.start_free ) {
	    // Get next transaction
	    rdesc = &client_shared->desc_ring[ ring_info.start_dirty ];
	    
	    dprintf(debug_ide, "IDE process server rdesc %x client data %x\n",
		    rdesc, rdesc->client_data);

	    
	    if( rdesc->status.X.server_owned) 
		
		break;
	    // TODO: signal error in IDE part
	    if( rdesc->status.X.server_err ) 
	    {
		DEBUGGER_ENTER("error");
	    } 
	    else 
 	    {
		dev = (ide_device_t*)rdesc->client_data;
		ASSERT(dev);
		
		switch(dev->data_transfer) 
		{

		case IDE_CMD_DATA_IN:
		    dev->set_data_wait();
		    dev->raise_irq();
		    break;

		case IDE_CMD_DATA_OUT:
		    if(dev->req_sectors>0)
			dev->set_data_wait();
		    else
			dev->set_cmd_wait();
		    dev->raise_irq();
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
		    dev->set_cmd_wait();
		    dev->raise_irq();
		}
		break;
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
		    dev->set_cmd_wait();
		    dev->raise_irq();
		}
		break;
#endif
		default:
		    printf("IDE unhandled transfer type %d\n", dev->data_transfer); 
		    DEBUGGER_ENTER("Unhandled transfer type");
		}

	    }
	    // Move to next 
	    ring_info.start_dirty = (ring_info.start_dirty + 1) % ring_info.cnt;
	} /* while */
	ide_release_lock(&ring_info.lock);

    } /* for */
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
    dprintf(debug_ide, "IDE sending request block %x size %d page %x (%c)\n",   
	    block, size, data, (write ? 'W' : 'R'));
    
    msg_virq_build(client_shared->server_irq_no, L4_nilthread);
    backend_notify_thread(client_shared->server_irq_tid, L4_Never);
}


/* additional dma client code for dd/os
 * Note: In the context of DMA controllers, write transfers move data from a device
 * to memory whereas here write transfers data from memory to a device.
 */
void ide_t::l4vm_transfer_dma( u32_t block, ide_device_t *dev, void *dsct , bool write)
{
    dprintf(debug_ide, "IDE l4vm transfer dma\n");

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
    for( n=0 ; n < 512 ; n++) 
    {
	client_shared->dma_vec[n].size = pe->transfer.fields.count;
	client_shared->dma_vec[n].page = (void**)pe->base_addr;
	dprintf(debug_ide,  "IDE transfer %d bytes to %x\n", 
		client_shared->dma_vec[n].size, 
		client_shared->dma_vec[n].page);
	if(pe->transfer.fields.eot)
	    break;
	pe++;
    }
    rdesc->count = n+1;
    rdesc->client_data = (void**)dev;
    rdesc->status.raw = 0;
    rdesc->status.X.do_write = write;
    rdesc->status.X.speculative = 0;
    rdesc->status.X.server_owned = 1;

    server_shared->irq_status |= 1; // was L4VMBLOCK_SERVER_IRQ_DISPATCH
    server_shared->irq_pending = true;
    dprintf(debug_ide, "IDE sending request for block %d with %d entries (%c)\n",
	    block, n+1, ( write ? 'W' : 'R'));
    
    msg_virq_build(client_shared->server_irq_no, L4_nilthread);
    backend_notify_thread(client_shared->server_irq_tid, L4_Never);
#endif
}


// Called from i82371ab when guest enables/starts dma bus master operation
void ide_t::ide_start_dma( ide_device_t *dev, bool write)
{
    dprintf(debug_ide, "IDE start dma\n");

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
	dev->raise_irq();
	printf( "IDE prd too small\n");
	printf( "IDE size %d request %d\n", size, (dev->req_sectors*IDE_SECTOR_SIZE));
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

void ide_channel_t::write_register(u16_t reg, u16_t value)
{
    dprintf(debug_ide_reg, "IDE %d/%d write reg %C val %x\n", 
	    chnum, current, DEBUG_TO_4CHAR(ide_reg_to_str_write[reg]), value);
    
   
    switch(reg) {
	
    case 0: 
	dev[current].io_data_write(value);
	break;
    case 1:
	dev[0].reg_feature = value;
	dev[1].reg_feature = value;
	break;
    case 2:
	dev[0].reg_nsector = value;
	dev[1].reg_nsector = value;
	break;
    case 3:
	dev[0].reg_lba_low = value;
	dev[1].reg_lba_low = value;
	break;
    case 4:
	dev[0].reg_lba_mid = value;
	dev[1].reg_lba_mid = value;
	break;
    case 5:
	dev[0].reg_lba_high = value;
	dev[1].reg_lba_high = value;
	break;
    case 6:
	dev[0].reg_device.raw = value;
	dev[1].reg_device.raw = value;
	current = (value & 0x10) ? 1 : 0;
	break;
    case 7:
	// ignore commands to non existent devices
	dev->command( value );
	break;
    case 14:
	// srst set to one, set busy
	if( !dev[0].reg_dev_control.x.srst && (value & 0x4)) {
	    dev[0].reg_status.x.bsy = 1;
	    dev[1].reg_status.x.bsy = 1;
	} 
	// srst cleared to zero
	else if( dev[0].reg_dev_control.x.srst && !(value & 0x4)) {
	    dev[0].reg_status.x.bsy = 0;
	    dev[1].reg_status.x.bsy = 0;
	    software_reset();
	}
	dev[0].reg_dev_control.raw = value;
	dev[1].reg_dev_control.raw = value;
	break;
    default: 
	printf( "IDE %d/%d write to unknown register %d\n", chnum, current, reg);
    }
}


u16_t ide_channel_t::read_register(u16_t reg)
{
    u16_t value = 0;
    
    if (!dev[current].present)
 	return value;
   
    switch(reg) 
    {
    case 0: 
	value = dev[current].io_data_read();
	break;
    case 1: 
	value = dev[current].reg_error.raw;
	break;
    case 2: 
	value = dev[current].reg_nsector;
	break;
    case 3: 
	value = dev[current].reg_lba_low;
	break;
    case 4: 
	value = dev[current].reg_lba_mid;
	break;
    case 5: 
	value = dev[current].reg_lba_high;
	break;
    case 6: 
	value = dev[current].reg_device.raw;
	break;
    case 7: 
	value = dev[current].reg_status.raw;
	break;
    case 14: 
	value = dev[current].reg_status.raw;
	break;
    default:	
	printf( "IDE read from unknown register %d\n", reg);
	value = 0;
	break;
    }
    dprintf(debug_ide_reg, "IDE %d/%d read reg %C val %x\n", 
	    chnum, current, DEBUG_TO_4CHAR(ide_reg_to_str_write[reg]), value);

    return value;
}


// 9.2, p. 315
void ide_channel_t::software_reset()
{
    dprintf(debug_ide, "IDE %d software reset present %d/%d\n", chnum,
	    dev[0].present, dev[1].present);
    
    if(dev[0].present) 
    {
	if(!dev[1].present)
	    dev[0].reg_error.x.bit7 = 0;
	else
	    dev[0].reg_device.x.dev = 0;
	dev[0].reg_status.raw &= ~(0xD);
	dev[0].set_signature();
    }
    
    if(dev[1].present) 
    {
	dev[1].reg_device.x.dev = 0;
	dev[1].reg_status.raw &= ~(0xD);
	dev[1].set_signature();
    }
}


void ide_device_t::read_packet()
{
    dprintf(debug_ide, "IDE read packet\n");
    if (!cdrom)
    {
	printf("IDE read packet command -- not a CD-ROM\n");
	abort_command(0);
	return;
    }
    //u16_t *dptr = (u16_t*) (dev->io_buffer+dev->io_buffer_index); 
    //u16_t dat = *dptr;
    
    printf( "IDE unimplemented read packet (CD-ROM) idx %d\n", io_buffer_index);
    DEBUGGER_ENTER("UNIMPLEMENTED");

}

// 8.34, p. 199
void ide_device_t::read_sectors()
{
    dprintf(debug_ide, "IDE read sectors\n");
    u32_t sector = 0, n;

    n = req_sectors = ( reg_nsector ? reg_nsector : 256 );

    if( reg_device.x.lba )
    {
	sector = get_sector();
    }
    else 
    {
	printf( "IDE unimplemented read via C/H/S\n");
	DEBUGGER_ENTER("UNIMPLEMENTED");
    }

    if( n > IDE_IOBUFFER_BLOCKS)
	n = IDE_IOBUFFER_BLOCKS;
    
    ide->l4vm_transfer_block( sector, n*IDE_SECTOR_SIZE, (void*)io_buffer_dma_addr, false, this);
    
    data_transfer = IDE_CMD_DATA_IN;
    io_buffer_index = 0;
    io_buffer_size = IDE_SECTOR_SIZE * n;
    req_sectors -= n;
    set_sector( sector + n );
    reg_status.x.bsy = 1;
}



// 8.62, p. 303
void ide_device_t::write_sectors()
{
    dprintf(debug_ide, "IDE write sectors\n");
    u32_t sector, n;

    n = req_sectors = ( reg_nsector ? reg_nsector : 256 );

    if( reg_device.x.lba ) 
    {
	sector = get_sector();
    }
    else 
    {
	printf( "IDE unimplemented write via C/H/S\n");
	DEBUGGER_ENTER("UNIMPLEMENTED");
    }


    if( n > IDE_IOBUFFER_BLOCKS )
	n = IDE_IOBUFFER_BLOCKS;

    data_transfer = IDE_CMD_DATA_OUT;
    io_buffer_size = IDE_SECTOR_SIZE * n;
    io_buffer_index = 0;
    req_sectors -= n;

    set_data_wait();
    reg_status.x.drdy=1;
    raise_irq();
}



/* 8.15, p. 113 */
void ide_device_t::identify( )
{
    memset(io_buffer, 0, 512);
    u16_t *buf = (u16_t*)io_buffer;

    // fill buffer with identify information
    *(buf) = 0xff02;
    
    // obsolete since ata-6, but most OSes use it nevertheless
    
    *(buf+1) = chs.cylinder;
    *(buf+3) = chs.head;
    *(buf+6) = chs.sector;

    padstr( (char*)(buf+10), (char*)serial, 20 );
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
    *(buf+60) = lba_sector;
    *(buf+61) = lba_sector >> 16;
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
    if(dma)
	*(buf+88) = 7 | (1 << (udma_mode + 8)); // udma support
    else
	*(buf+88) = 7; 
    *(buf+93) = (1 << 14) | 1;
    *(buf+100) = 0; // lba48
    
    set_data_wait();

    // setup data transfer
    io_buffer_index = 0;
    io_buffer_size = IDE_SECTOR_SIZE;
    data_transfer = IDE_CMD_DATA_IN;
}

void ide_device_t::identify_cdrom()
{
    if (!cdrom)
    {
	printf("IDE identify CD-ROM command -- not a CD-ROM\n");
	abort_command(0);
	return;
    }

    memset(io_buffer, 0, 512);
    u16_t *buf = (u16_t*)io_buffer;

    if (present)
    {
	// fill buffer with identify information
	*(buf) = 0x0503;
    
	// obsolete since ata-6, but most OSes use it nevertheless
	*(buf+1) = chs.cylinder;
	*(buf+3) = chs.head;
	*(buf+6) = chs.sector;

	padstr( (char*)(buf+10), (char*)serial, 20 );
	padstr( (char*)(buf+23), "l4ka 0.1", 8 );
	padstr( (char*)(buf+27), "L4KA AFTERBURNER CD-ROM", 38 );
    
	*(buf+47) = 0x8000 | IDE_MAX_READ_WRITE_MULTIPLE;
	*(buf+49) = 0x0300;
#if defined(CONFIG_DEVICE_I82371AB)
	*(buf+53) = 6;
#else
	*(buf+53) = 2;
#endif
	*(buf+59) = 0x0100 | 1 ;
	*(buf+60) = lba_sector;
	*(buf+61) = lba_sector >> 16;
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
	if(dma)
	    *(buf+88) = 7 | (1 << (udma_mode + 8)); // udma support
	else
	    *(buf+88) = 7; 
	*(buf+93) = (1 << 14) | 1;
	*(buf+100) = 0; // lba48
    }
    set_data_wait();

    // setup data transfer
    io_buffer_index = 0;
    io_buffer_size = IDE_SECTOR_SIZE;
    data_transfer = IDE_CMD_DATA_IN;
}

// 8.25, p. 166
void ide_device_t::read_dma()
{
    dprintf(debug_ide, "IDE read dma\n");

#if defined(CONFIG_DEVICE_I82371AB)
    
    if(!dma) { // dma disabled
	abort_command(0);
	raise_irq();
	return;
    }

    req_sectors = ( reg_nsector ? reg_nsector : 256 );
    //    printf( n << " sectors\n");
    i82371ab_t::get_device(dev_num)->register_device(dev_num, this);

    data_transfer = IDE_CMD_DMA_IN;
    set_data_wait();

#else 
    // no dma support, abort
    abort_command(0);
    raise_irq();
#endif
}


void ide_device_t::write_dma()
{
    dprintf(debug_ide, "IDE write dma\n");
#if defined(CONFIG_DEVICE_I82371AB)
    if(!dma) { // dma disabled
	abort_command(0);
	raise_irq();
	return;
    }

    req_sectors = ( reg_nsector ? reg_nsector : 256 );

    i82371ab_t::get_device(dev_num)->register_device(dev_num, this);

    data_transfer = IDE_CMD_DMA_OUT;
    set_data_wait();

#else 
    // no dma support, abort
    abort_command(0);
    raise_irq();
#endif
}


// 8.46, p. 224
void ide_device_t::set_features()
{
    dprintf(debug_ide, "IDE set features\n");
	
    switch(reg_feature) 
	{
	case 0x03 : // set transfer mode
	    switch(reg_nsector >> 3) 
		{
		case 0x0: // pio default mode
		    dma=0;
		    break;

		case 0x8: // udma mode
		    dma=1;
		    udma_mode = reg_nsector & 0x7;
		    break;

		default:
		    abort_command(0);
		    return;
		}
	    break;

	    
	default:
	    abort_command(0);
	    return;
	}
    set_cmd_wait();
}


void ide_device_t::raise_irq()
{
    if(!reg_dev_control.x.nien)
    {
	dprintf(debug_ide, "IDE raise irq %d\n", ch->irq);
	get_intlogic().raise_irq(ch->irq);
    }
}


void ide_t::ide_portio( u16_t port, u32_t & value, bool read )
{
    u16_t reg = port & 0xF;// + ((port & 0x200)>>8) 

    if(read)
	switch( port & ~(0xf)) 
	{
	case 0x1F0:
	    value = channel[0].read_register(reg);
	    break;
	case 0x3F0:
	    value = channel[0].read_register(reg+8);
	    break;
	case 0x170:
	    value = channel[1].read_register(reg);
	    break;
	case 0x370:
	    value = channel[1].read_register(reg+8);
	    break;
	default:
	    printf( "IDE read from unknown port %d\n", port);

	}
    else
	switch(port & ~(0xf)) 
	{
	case 0x1F0:
	    channel[0].write_register(reg, value );
	    break;
	case 0x3F0:
	    channel[0].write_register(reg+8, value );
	    break;
	case 0x170:
	    channel[1].write_register(reg, value );
	    break;
	case 0x370:
	    channel[1].write_register(reg+8, value );
	    break;
	default:
	    printf( "IDE write to unknown port %d\n", port);
	}
}


u16_t ide_device_t::io_data_read()
{
    if(data_transfer != IDE_CMD_DATA_IN) {
	printf( "IDE read with no data request\n");
	return 0xff;
    }

    u16_t data = *((u16_t*)(io_buffer+io_buffer_index));

    io_buffer_index += 2;
    
    dprintf(debug_ide+3, "IDE io data read %x idx %d size %d val %x\n", 
	    io_buffer, io_buffer_index, io_buffer_size, data);
    
    if(io_buffer_index >= io_buffer_size) 
    { 
	dprintf(debug_ide, "IDE read sector complete %x size %x req_sectors %d\n", 
		io_buffer, io_buffer_size, req_sectors);
	
        // transfer complete
	if( req_sectors > 0) 
	{
	    u32_t n = req_sectors;
	    if( n > IDE_IOBUFFER_BLOCKS)
		n = IDE_IOBUFFER_BLOCKS;
	    ide->l4vm_transfer_block( get_sector(), n*IDE_SECTOR_SIZE, 
				      (void*)io_buffer_dma_addr, false, this);
	    req_sectors -= n;
	    io_buffer_index = 0;
	    io_buffer_size = n * IDE_SECTOR_SIZE;
	    set_sector( get_sector() + n);
	    reg_status.x.bsy = 1;
	}
	else 
	{
	    data_transfer = IDE_CMD_DATA_NONE;
	    set_cmd_wait();
	}
	return data;
    }
    // TODO: check if raising the INT here is too early
    if( (io_buffer_index % IDE_SECTOR_SIZE)==0) // next DRQ data block
	raise_irq();
    return data;
}


void ide_device_t::io_data_write(u16_t value )
{
    if(data_transfer != IDE_CMD_DATA_OUT) 
    { 
        // do nothing
	printf( "IDE write with no data request\n");
	return;
    }
    
    dprintf(debug_ide, "IDE io data write %x idx %d\n", value, io_buffer_index);
    
    *((u16_t*)(io_buffer+io_buffer_index)) = value;
    io_buffer_index += 2;

    if(io_buffer_index >= io_buffer_size) 
    {
	if( req_sectors > 0 ) {
	    u32_t n = req_sectors;
	    if( n > IDE_IOBUFFER_BLOCKS )
		n = IDE_IOBUFFER_BLOCKS;
	    ide->l4vm_transfer_block( get_sector(), n*IDE_SECTOR_SIZE, (void*)io_buffer_dma_addr, true, this);
	    req_sectors -= n;
	    io_buffer_index = 0;
	    io_buffer_size = n * IDE_SECTOR_SIZE;
	    set_sector( get_sector() + n );
	    reg_status.x.bsy = 1;
	}
	else {
	    reg_status.x.bsy = 1;
	    ide->l4vm_transfer_block( get_sector(), io_buffer_size, (void*)io_buffer_dma_addr, true, this);
	}
	return;
    }

    // TODO: see above
    if( (io_buffer_index % IDE_SECTOR_SIZE)==0) // next DRQ data block
	raise_irq();
}


void ide_device_t::command(u16_t cmd)
{
    ASSERT(present);

    switch(cmd) 
    {
	
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
	dprintf(debug_ide, "IDE identify command\n");
	identify(); 
	raise_irq();
	return;

    case IDE_CMD_IDENTIFY_PACKET_DEVICE:
	dprintf(debug_ide, "IDE identify CDROM command\n");
	identify_cdrom(); 
	raise_irq();
	return;
	
    case IDE_CMD_IDLE:
    case IDE_CMD_IDLE_IMMEDIATE:
    case IDE_CMD_MEDIA_EJECT:
    case IDE_CMD_MEDIA_LOCK:

    case IDE_CMD_MEDIA_UNLOCK:
    case IDE_CMD_NOP:
    case IDE_CMD_PACKET:
	read_packet();
	break;		
    case IDE_CMD_READ_BUFFER:
	break;
    case IDE_CMD_READ_DMA:
	read_dma();
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
	read_sectors();
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
	set_features();
	raise_irq();
	return;

    case IDE_CMD_SET_MAX:
    case IDE_CMD_SET_MAX_ADDRESS_EXT:
	break;

    case IDE_CMD_SET_MULTIPLE_MODE:
	// TODO: check for power of 2
	if(reg_nsector > IDE_MAX_READ_WRITE_MULTIPLE)
	    abort_command(0);
	else {
	    mult_sectors = reg_nsector;
	    set_cmd_wait();
	}
	raise_irq();
	return;

    case IDE_CMD_SLEEP:
    case IDE_CMD_SMART:
    case IDE_CMD_STANDBY:
    case IDE_CMD_STANDBY_IMMEDIATE:
    case IDE_CMD_WRITE_BUFFER:
	break;
    case IDE_CMD_WRITE_DMA:
	write_dma();
	return;
    case IDE_CMD_WRITE_DMA_EXT:
    case IDE_CMD_WRITE_DMA_QUEUED:
    case IDE_CMD_WRITE_DMA_QUEUED_EXT:
    case IDE_CMD_WRITE_LOG_EXT:
    case IDE_CMD_WRITE_MULTIPLE:

    case IDE_CMD_WRITE_MULTIPLE_EXT:
	break;
    case IDE_CMD_WRITE_SECTORS:
	write_sectors();
	return;
    case IDE_CMD_WRITE_SECTORS_EXT:
	break;

	// linux keeps calling this old stuff, simply do nothing
    case IDE_CMD_INITIALIZE_DEVICE_PARAMETERS:
    case IDE_CMD_RECALIBRATE:
	set_cmd_wait();
	raise_irq();
	return;

    default:
	printf( "IDE unknown command %x\n", cmd);
	abort_command(0);
	raise_irq();
	return;
    }
    printf( "IDE unsupported command %x\n", cmd);
    abort_command(0);
    raise_irq();
}

void ide_portio( u16_t port, u32_t & value, bool read )
{
#if defined(CONFIG_DEVICE_IDE)
    ide.ide_portio( port, value, read);
#endif
}
#endif


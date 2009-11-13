/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_energy.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __ENERGY_H__
#define __ENERGY_H__

#include "block.h"
#include "server.h"
#include <linux/types.h>

#define FD_DISK 0
#define FD_FLASH 1

#define ENTRY_READ	0x1
#define ENTRY_WRITE	0x2

// number of consecutive sectors that form an object
#define OBJECT_SIZE	32	// 16KB
#define OBJECT_MASK	~(0x1F)

#define BLOCK_MASK(offset,size)	(((1ULL<<size)-1)<<offset)

typedef struct EnergyRequest {
    int fd;
    uint64_t sector_num;
    int size;
    uint8_t *buf;
} EnergyRequest;

int energy_init(int fd_disk, int fd_flash);
int64_t energy_getlength(void);
int energy_readtranslation(EnergyRequest *req);
int energy_writetranslation(EnergyRequest *req);
int energy_shutdown(void);

typedef struct EnergyState {
    int fd_disk, fd_flash;
    
} EnergyState;

typedef struct CacheEntry {
    /* sector on hard disk */
    uint64_t hd_sector;
    /* sector on flash disk */
    uint64_t fd_sector;
    /* mask to identify used sectors */
    uint32_t mask;
    /* some status variable */
    uint32_t state;

} CacheEntry;

#endif /* __ENERGY_H__ */

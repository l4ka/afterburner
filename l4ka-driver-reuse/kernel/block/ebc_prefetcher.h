/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_prefetcher.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#ifndef __PREFETCHER_H__
#define __PREFETCHER_H__

#include "block.h"
#include "server.h"
#include <linux/types.h>

#define TRACK_SIZE	32
#define TRACK_MASK	~(0x1F)

int prefetcher_init(void);
int prefetcher_load_entries(uint64_t base, uint32_t count);


#endif /* __PREFETCHER_H__ */

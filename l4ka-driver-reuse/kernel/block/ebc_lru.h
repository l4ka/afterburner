/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_lru.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __LRU_H__
#define __LRU_H__

#include "ebc_energy.h"

int lru_init(void);
void lru_handle_read(uint64_t key);
int lru_load_cache(uint32_t max);    
void lru_dump_stats(void);



#endif /* __LRU_H__ */

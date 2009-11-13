/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_trace.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef BLOCK_TRACE_H
#define BLOCK_TRACE_H

#include "block.h"
#include "server.h"
#include <linux/types.h>

//#define BLOCK_TRACE 1

static __inline__ uint64_t x86_rdtsc(void)
{
    uint32_t eax, edx;

    __asm__ __volatile__ (
	"rdtsc" : "=a"(eax), "=d"(edx));

    return (((uint64_t)edx)<<32) | (uint64_t)eax;
}

#ifdef BLOCK_TRACE

#define BLOCK_TRACE_INIT(fd)	init_block_trace(fd)
#define BLOCK_TRACE_ACCESS(fd,block,size,rw)	trace_block_access(fd,block,size,rw)
#define BLOCK_TRACE_READ(fd,block,size)	BLOCK_TRACE_ACCESS(fd,block,size,0)
#define BLOCK_TRACE_WRITE(fd,block,size)	BLOCK_TRACE_ACCESS(fd,block,size,1)

#else 

#define BLOCK_TRACE_INIT(fd)
#define BLOCK_TRACE_ACCESS(fd,block,size,rw)
#define BLOCK_TRACE_READ(fd,block,size)
#define BLOCK_TRACE_WRITE(fd,block,size)

#endif

void init_block_trace(int *fd);
void trace_block_access(int fd, uint64_t block, uint32_t size, uint8_t rw);

int block_translation(uint64_t block);

#endif /* BLOCK_TRACE_H */

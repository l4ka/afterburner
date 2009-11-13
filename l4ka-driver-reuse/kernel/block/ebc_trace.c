/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_trace.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "ebc_trace.h"


void init_block_trace(int *fd)
{
    BUG();
    L4_KDB_Enter("Implement me");
#if 0

    char template[] = "/tmp/fileXXXXXX";
    int fds;

    fds = mkstemp(template);
    dprintk(0, "using %s as trace file\n", template);
    
    *fd = fds;
#endif
}

void trace_block_access(int fd, uint64_t block, uint32_t size, uint8_t rw)
{
    dprintk(0, "%ld\t%ld\t%d\t%c\n", x86_rdtsc(), block, size, (rw) ? 'W' : 'R');
//    printf( "%ld %ld %d %c\n", rdtsc(), block, size, (rw) ? 'W' : 'R');
}


int block_translation(uint64_t block)
{
    return (block%2);
}

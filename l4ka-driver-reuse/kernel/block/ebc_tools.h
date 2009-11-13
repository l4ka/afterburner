/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_tools.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __TOOLS_H__
#define __TOOLS_H__

#include <linux/types.h>

static void hdstandby(int fd)
{
    
    BUG();
    L4_KDB_Enter("Implement me");
    
#if 0
    unsigned char args1[4] = {WIN_STANDBYNOW1,0,0,0};
    unsigned char args2[4] = {WIN_IDLEIMMEDIATE,0,0,0};
    
    if(ioctl(fd, HDIO_DRIVE_CMD, args1))
    {
        printf("ioctl error\n");
    }
#endif
}


#endif /* __TOOLS_H__ */

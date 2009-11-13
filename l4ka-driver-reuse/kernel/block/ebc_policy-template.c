/*********************************************************************
 *                
 * Copyright (C) 2009,  Karlsruhe University
 *                
 * File path:     ebc_policy-template.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "ebc_energy.h"

static int fdes_disk, fdes_flash;

int energy_init(int fd_disk, int fd_flash)
{
  fdes_disk = fd_disk;
  fdes_flash = fd_flash;

  return 0;
}

int energy_readtranslation(EnergyRequest *req)
{
  req->fd = FD_DISK;

  return FD_DISK;
}


int energy_writetranslation(EnergyRequest *req)
{
  req->fd = FD_DISK;

  return FD_DISK;
}

int64_t energy_getlength(void)
{

    BUG();
    L4_KDB_Enter("Implement me");
    return 0;
#if 0
    int64_t dsize, fsize;
    dsize = lseek(fdes_disk, 0, SEEK_END);
    fsize = lseek(fdes_flash, 0, SEEK_END);
    return dsize;
#endif
}

int energy_shutdown(void)
{

  return 0;
}

/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     l4ka/user.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __L4KA__USER_H__
#define __L4KA__USER_H__

#if defined(CONFIG_L4KA_VMEXT)
#include INC_WEDGE(vmext/user.h)
#elif defined(CONFIG_L4KA_VT)
#include INC_WEDGE(hvm/user.h)
#else 
#include INC_WEDGE(vm/user.h)
#endif
#endif /* !__L4KA__USER_H__ */

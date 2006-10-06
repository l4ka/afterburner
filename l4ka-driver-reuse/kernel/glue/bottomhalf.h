#ifndef __LINUXGLUE__BOTTOMHALF_H__
#define __LINUXGLUE__BOTTOMHALF_H__

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,41)
# include <linux/tqueue.h>
#else
# include <linux/workqueue.h>
# define INIT_TQUEUE	INIT_WORK
# define schedule_task	schedule_work
# define tq_struct	work_struct
#endif

#endif	/* __LINUXGLUE__BOTTOMHALF_H__ */

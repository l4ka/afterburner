
This directory contains a Linux kernel module that enables performance
counters, for use with native Linux.  It uses the performance monitor
configuration of the L4 hypervisor task.

Either load it as a dynamic module, or link it directly to the kernel.

To link directly against Linux 2.6:
1.  Either define a constant called CONFIG_UKA_PERFMON_H pointing at the 
    hypervisor/include/ia32/perfmon.h file, or create a file called 
    perfmon_config.h that includes the hypervisor/include/ia32/perfmon.h 
    file.  Consult the source code for clarification.
2.  Create a symbolic link from linux-2.6/arch/i386/perfmon to this directory,
    vm/benchmarks/kmod/perfmon
3.  In linux-2.6/arch/i386/Makefile, add the lines
      drivers-$(CONFIG_MPENTIUM4)     += arch/i386/perfmon/
      drivers-$(CONFIG_MK8)           += arch/i386/perfmon/
    The terminating slashes are important!
4.  Build the Linux kernel.


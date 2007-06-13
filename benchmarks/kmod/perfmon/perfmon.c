
#include <linux/config.h>
#include <linux/module.h>
#include <asm/msr.h>

#ifdef CONFIG_UKA_PERFMON_H
#include CONFIG_UKA_PERFMON_H
#else
// Create the following file, and in it, include the hypervisor's perfmon.h. 
#include "perfmon_config.h"
#endif

#define AMD64_HWCR_MSR		0xC0010015	/* HW configuration MSR */
#define AMD64_HWCR_FFDIS	(1 <<  6)	/* Flush filter disable*/ 

typedef unsigned int u32_t;
typedef unsigned long long u64_t;


static inline u64_t 
amd64_rdmsr(const u32_t reg)
{
	u32_t __eax, __edx;

	__asm__ __volatile__ (
		"rdmsr"
		: "=a"(__eax), "=d"(__edx)
		: "c"(reg)
	);

	return ( (((u64_t) __edx) << 32) | ( (u64_t) __eax));
}

static inline void
amd64_wrmsr(const u32_t reg, const u64_t val)
{   
	__asm__ __volatile__ (
		"wrmsr"
		: 
		: "a"( (u32_t) val), "d" ( (u32_t) (val >> 32)), "c" (reg));
}


static inline int
is_amd64_flushfilter_enabled(void)
{
	return !(amd64_rdmsr(AMD64_HWCR_MSR) & AMD64_HWCR_FFDIS);
}

static inline void
amd64_flushfilter_enable(void)
{
	u64_t hwcr = amd64_rdmsr(AMD64_HWCR_MSR);
	amd64_wrmsr(AMD64_HWCR_MSR, hwcr & ~AMD64_HWCR_FFDIS);
}

static void do_wrmsr( const unsigned int reg, const unsigned long long val )
{
    wrmsrl( reg, val );
}

static int __init
uka_perfmon_init(void)
{
#if !defined(CONFIG_XEN)
	printk(KERN_INFO "UKa: Enabling user-level performance counters.\n");
        set_in_cr4(X86_CR4_PCE);
#endif
        switch (ia32_cpu_type()) {
        case BMT_P4:
                printk(KERN_INFO "UKa: Pentium 4 detected, enabling P4 "
			"performance counting.\n");
                perfmon_setup(&p4_benchmarks, do_wrmsr);
                break;
        case BMT_K8:
                printk(KERN_INFO "UKa: AMD K8 detected, enabling K8 "
			"performance counting.\n");
                perfmon_setup(&k8_benchmarks, do_wrmsr);
//		if (!is_amd64_flushfilter_enabled())
//		{
//			printk(KERN_INFO "UKa: enabling AMD flush filter.\n");
//			amd64_flushfilter_enable();
//		}
                break;
        default:
                printk(KERN_WARNING "UKa: Unknown CPU type, performance "
			"monitoring not configured.\n");
                break;
        }

	return 0;
}

static void __exit
uka_perfmon_exit( void )
{
}

module_init(uka_perfmon_init);
module_exit(uka_perfmon_exit);

MODULE_AUTHOR( "Joshua LeVasseur <jtl@ira.uka.de>" );
MODULE_DESCRIPTION( "Enable UKa performance monitoring." );
MODULE_LICENSE( "Dual BSD/GPL" );
MODULE_VERSION( "monkey" );


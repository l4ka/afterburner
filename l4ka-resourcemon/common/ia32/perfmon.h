/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     l4ka-resourcemon/common/ia32/perfmon.h
 * Description:   
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/
#ifndef __L4KA_RESOURCEMON__COMMON__IA32__PERFMON_H__
#define __L4KA_RESOURCEMON__COMMON__IA32__PERFMON_H__

/** @file
 * Performance monitoring support for P4 and K8 (Opteron).
 * Independent of resourcemon and used in benchmarking tools and the
 * native Linux kernel, too.
 */

typedef union
{
    unsigned long long raw;
    struct {
	unsigned int rsv0 : 2;
	unsigned int usr : 1;
	unsigned int os : 1;
	unsigned int tag_enable : 1;
	unsigned int tag_value : 4;
	unsigned int event_mask : 16;
	unsigned int event_select : 6;
	unsigned int rsv1 : 1;
	unsigned int rsv2 : 32;
    } X __attribute__ ((packed));
} escr_t;

typedef union
{
    unsigned long long raw;
    struct {
	unsigned int rsv0 : 12;
	unsigned int enable : 1;
	unsigned int escr_select : 3;
	unsigned int rsv1 : 2; // must be set to 11B!
	unsigned int compare : 1;
	unsigned int complement : 1;
	unsigned int threshold : 4;
	unsigned int edge : 1;
	unsigned int force_ovf : 1;
	unsigned int ovf_pmi : 1;
	unsigned int rsv2 : 3;
	unsigned int cascade : 1;
	unsigned int ovf : 1;
	unsigned int rsv3 : 32;
    } X __attribute__ ((packed));
} cccr_t;

typedef struct {
    unsigned int escr_addr;
    unsigned int pc_addr;
    unsigned int cccr_addr;
} reg_set_t;

typedef struct {
    reg_set_t regs[4];
    unsigned int escr_select;	// CCCR Select in the manuals
    unsigned int event_select;
    unsigned int event_mask;
} metric_t;

// the metrics are placed here so this header file can be used in the
// resourcemon and in perfmon and to avoid code duplication

// missing (and bitches to set up):
//   2&3nd-Level Cache Read Misses
//   1st level cache load misses retired

static const metric_t m_l2l3_misses = {
    regs: {	// BSU_CR_ESCR0, 1
	{ 0x3a0, 0x300, 0x360 },
	{ 0x3a0, 0x301, 0x361 },
	{ 0x3a1, 0x302, 0x362 },
	{ 0x3a1, 0x303, 0x363 } },
    escr_select: 0x7 , event_select: 0x0c, event_mask: (1<<8)|(1<<9) };

static const metric_t m_l3_misses = {
    regs: {	// BSU_CR_ESCR0, 1
	{ 0x3a0, 0x300, 0x360 },
	{ 0x3a0, 0x301, 0x361 },
	{ 0x3a1, 0x302, 0x362 },
	{ 0x3a1, 0x303, 0x363 } },
    escr_select: 0x7 , event_select: 0x0c, event_mask: (1<<9) };

static const metric_t m_l2_misses = {
    regs: {	// BSU_CR_ESCR0, 1
	{ 0x3a0, 0x300, 0x360 },
	{ 0x3a0, 0x301, 0x361 },
	{ 0x3a1, 0x302, 0x362 },
	{ 0x3a1, 0x303, 0x363 } },
    escr_select: 0x7 , event_select: 0x0c, event_mask: (1<<8) };

// page table walks due to DTLB misses
static const metric_t m_dpage_walks = {
     regs: {
         { 0x3ac, 0x300, 0x360 },
	 { 0x3ac, 0x301, 0x361 },
         { 0x3ad, 0x302, 0x362 },
         { 0x3ad, 0x303, 0x363 } },
     escr_select: 0x4, event_select: 0x1, event_mask: 0x1 };

// page table walks due to ITLB misses
static const metric_t m_ipage_walks = {
    regs: {
	{ 0x3ac, 0x300, 0x360 },
        { 0x3ac, 0x301, 0x361 },
        { 0x3ad, 0x302, 0x362 },
        { 0x3ad, 0x303, 0x363 } },
    escr_select: 0x4, event_select: 0x1, event_mask: 0x2 };

// branch misprediction ratio: mispred brnch ret / brnch ret
static const metric_t m_branch_retired = {
    regs: {
	{ 0x3c2, 0x304, 0x364 },
        { 0x3c2, 0x305, 0x365 },
        { 0x3c3, 0x306, 0x366 },
        { 0x3c3, 0x307, 0x367 } },
    escr_select: 0x2, event_select: 0x4, event_mask: 0xf };

static const metric_t m_mispred_branch_retired = {
    regs: {
	{ 0x3c2, 0x304, 0x364 },
        { 0x3c2, 0x305, 0x365 },
        { 0x3c3, 0x306, 0x366 },
        { 0x3c3, 0x307, 0x367 } },
    escr_select: 0x2, event_select: 0x5, event_mask: 0xf };

// trace cache misses
static const metric_t m_trace_cache_misses = {
    regs: {
	{ 0x3b2, 0x300, 0x360 },
        { 0x3b2, 0x301, 0x361 },
        { 0x3b3, 0x302, 0x362 },
        { 0x3b3, 0x303, 0x363 } },
    escr_select: 0x0, event_select: 0x3, event_mask: 0x1 };

// 64KB cache aliasing conflicts
static const metric_t m_aliasing_conflicts = {
    regs: {
	{ 0x3a8, 0x308, 0x368 },
        { 0x3a8, 0x309, 0x369 },
        { 0x3a9, 0x30a, 0x36a },
        { 0x3a9, 0x30b, 0x36b } },
    escr_select: 0x5, event_select: 0x2, event_mask: 0x8 };

static const metric_t m_instr_retired = {
    regs: {
	{ 0x3b8, 0x30c, 0x36c },
	{ 0x3b8, 0x30d, 0x36d },
	{ 0x3b9, 0x30e, 0x36e },
	{ 0x3b9, 0x30f, 0x36f } },
    escr_select: 0x4, event_select: 0x2, event_mask: 0x3 };

// non-sleep cycles (special case in perfmon_setup_metric())
// cycles per instruction: non-sleep cycles / instr_retired
static const metric_t m_nonsleep_cycles = {
    regs: {
	{ 0x3bd, 0x311, 0x371 },
	{ 0x3bd, 0x311, 0x371 },
	{ 0x3bd, 0x311, 0x371 },
	{ 0x3bd, 0x311, 0x371 } },
    escr_select: 0x2, event_select: 0xFFFFFFFF, event_mask: 0xFFFFFFFF };



/**************** Opteron (K8) ******************/

#define K8_PESR_BASE 0xc0010000
#define K8_PMC_BASE  0xc0010004

typedef union {
    unsigned int raw;
    struct {
	unsigned int event_mask : 8;
	unsigned int unit_mask : 8;
	unsigned int usr : 1;
	unsigned int os : 1;
	unsigned int edge : 1;
	unsigned int pin_ctrl : 1;
	unsigned int intr : 1;
	unsigned int rsv0 : 1;
	unsigned int enable : 1;
	unsigned int inv : 1;
	unsigned int counter_mask : 8;
    } X __attribute__ ((packed));
} k8_pesr_t;

typedef struct {
    k8_pesr_t pesr;
} k8_metric_t;


static const k8_metric_t mk8_instr_retired = {
    pesr: { X: { event_mask: 0xc0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };

static const k8_metric_t mk8_cpu_clk_unhalted = {
    pesr: { X: { event_mask: 0x76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };

static const k8_metric_t mk8_dc_refill_from_system = {
    pesr: { X: { event_mask: 0x43, unit_mask: 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };

static const k8_metric_t mk8_dc_l1dtlb_l2dtlb_miss = {
    pesr: { X: { event_mask: 0x46, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };

static const k8_metric_t mk8_ic_l1itlb_l2itlb_miss = {
    pesr: { X: { event_mask: 0x85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };



/*************** Register Access *********************/

#if 0
extern inline void ia32_wrmsr(
	const unsigned int reg,
	const unsigned long long val )
{
    __asm__ __volatile__ (
	    "wrmsr"
	    :
	    : "A"(val), "c"(reg));
}
#endif

extern inline void ia32_cpuid(
	unsigned int func,
	unsigned int *eax,
	unsigned int *ebx,
	unsigned int *ecx,
	unsigned int *edx )
{
    __asm__ __volatile__ (
	    "cpuid"
	    : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
	    : "a" (func)
	    );
}

extern inline unsigned long long ia32_rdtsc( void )
{
    unsigned long long result;
    __asm__ __volatile__ (
	    "rdtsc"
	    : "=A"(result) );
    return result;
}

extern inline unsigned long long ia32_rdpmc( unsigned which )
{
    unsigned long long result;
    __asm__ __volatile__ (
	    "rdpmc"
	    : "=A"(result) : "c"(which) );
    return result;
}


/************** Complex Types **************************/

typedef struct
{
    union {
	const metric_t *p4;
	const k8_metric_t *k8;
    } metric;
    unsigned int regs;
    unsigned long long counter;
    const char *name;
} benchmark_t;

enum bm_type_t { BMT_UNKNOWN, BMT_P4, BMT_K8 };

typedef struct
{
    unsigned long long tsc;
    enum bm_type_t type; // type of benchmark array elements
    unsigned int size; // nr of elements in the benchmarks array
    benchmark_t benchmarks[4];
} benchmarks_t;

static benchmarks_t p4_benchmarks = {
    tsc: 0,
    type: BMT_P4,
    size: 4,
    benchmarks:
    {
	{metric: {p4: &m_nonsleep_cycles},    1, 0, "active cycles"},
        {metric: {p4: &m_dpage_walks},        2, 0, "dtlb misses"},
	{metric: {p4: &m_ipage_walks},        0, 0, "itlb misses"},
	{metric: {p4: &m_l2_misses},          3, 0, "l2 misses"}
    }
};

static benchmarks_t k8_benchmarks = {
    tsc: 0,
    type: BMT_K8,
    size: 4,
    benchmarks:
    {
	{metric: {k8: &mk8_cpu_clk_unhalted}, 0, 0, "active cycles"},
 	{metric: {k8: &mk8_dc_l1dtlb_l2dtlb_miss}, 2, 0, "DTLB misses"},
 	{metric: {k8: &mk8_ic_l1itlb_l2itlb_miss}, 3, 0, "ITLB misses"},
 	{metric: {k8: &mk8_dc_refill_from_system}, 1, 0, "DC refills from system"}
    }
};

/********************* Convenience Functions ***************/

#ifdef __cplusplus
extern "C" {
#endif

int strcmp(const char *s1, const char *s2);
int printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

extern inline enum bm_type_t
ia32_cpu_type( void )
{
    unsigned int max, id[4], version, brand, reserved, features;
    id[3] = 0;

    ia32_cpuid( 0, &max, &id[0], &id[2], &id[1] );
    ia32_cpuid( 1, &version, &brand, &reserved, &features );
    if( !strcmp((char *)id, "GenuineIntel") && ((version >> 8) & 0xf) == 0xf )
	return BMT_P4;
    else if ( !strcmp((char *)id, "AuthenticAMD") )
	return BMT_K8;

    return BMT_UNKNOWN;
}

typedef void (*ia32_wrmsr_t)(const unsigned int reg, const unsigned long long val );

extern inline void
perfmon_setup( benchmarks_t *benchmarks, ia32_wrmsr_t do_wrmsr )
{
    unsigned int i;

    for( i = 0; i < benchmarks->size; i++ ) {
	int regs = benchmarks->benchmarks[i].regs;
	if( BMT_P4 == benchmarks->type ) {
	    const metric_t *metric = benchmarks->benchmarks[i].metric.p4;

	    escr_t escr = { raw: 0ULL };
	    cccr_t cccr = { raw: 0ULL };

	    escr.X.usr = 1;
	    escr.X.os = 1;
	    escr.X.event_mask = metric->event_mask;
	    escr.X.event_select = metric->event_select;

	    cccr.X.enable = 1;
	    cccr.X.escr_select = metric->escr_select;
	    cccr.X.rsv1 = 3;

	    if( 0xFFFFFFFF == metric->event_select &&
		0xFFFFFFFF == metric->event_mask ) {
		escr.X.event_mask = 0x3;
		escr.X.event_select = 0x2;
		cccr.X.compare = 0x1;
		cccr.X.threshold = 0xf;
		cccr.X.complement = 0x1;
	    }

	    do_wrmsr( metric->regs[regs].escr_addr, escr.raw );
	    do_wrmsr( metric->regs[regs].pc_addr, 0UL );
	    do_wrmsr( metric->regs[regs].cccr_addr, cccr.raw );
	} else if( BMT_K8 == benchmarks->type ) {
	    k8_pesr_t pesr = (k8_pesr_t){raw: benchmarks->benchmarks[i].metric.k8->pesr.raw};
	    pesr.X.usr = 1;
	    pesr.X.os = 1;
	    pesr.X.enable = 1;

	    do_wrmsr( K8_PESR_BASE + regs, pesr.raw );
	    do_wrmsr( K8_PMC_BASE + regs, 0 );
	}
    }
}

extern inline unsigned long long
perfmon_read( benchmark_t *b, enum bm_type_t type )
{
    if( BMT_P4 == type )
	return ia32_rdpmc( b->metric.p4->regs[b->regs].pc_addr - 0x300 );
    else if( BMT_K8 == type )
	return ia32_rdpmc( b->regs );

    return 0ULL;
}

extern inline void perfmon_start( benchmarks_t *benchmarks )
{
    unsigned int i;
    benchmarks->tsc = ia32_rdtsc();
    for( i = 0; i < benchmarks->size; i++ )
	benchmarks->benchmarks[i].counter = perfmon_read(&benchmarks->benchmarks[i],
							      benchmarks->type);
}

extern inline void perfmon_stop( benchmarks_t *benchmarks )
{
    unsigned int i;
    benchmarks->tsc = ia32_rdtsc() - benchmarks->tsc;
    for( i = 0; i < benchmarks->size; i++ ) {
	unsigned long long end = 
	    perfmon_read(&benchmarks->benchmarks[i], benchmarks->type);
	if( end >= benchmarks->benchmarks[i].counter )
	    benchmarks->benchmarks[i].counter = end - benchmarks->benchmarks[i].counter;
	else // Wrap-around, and we assume only a single wrap-around.
	    benchmarks->benchmarks[i].counter = (~0ULL - benchmarks->benchmarks[i].counter) + end + 1;
    }
}

extern inline void perfmon_print_headers( benchmarks_t *benchmarks )
{
    unsigned int i;
    printf( "cycles" );
    for( i = 0; i < benchmarks->size; i++ )
	printf( ",%s", benchmarks->benchmarks[i].name );
    printf( "\n" );
}

extern inline void perfmon_print_data( benchmarks_t *benchmarks )
{
    unsigned int i;
    printf( "%llu", benchmarks->tsc );
    for( i = 0; i < benchmarks->size; i++ )
	printf( ",%llu", benchmarks->benchmarks[i].counter );
    printf( "\n" );
}

extern inline void perfmon_print_data32( benchmarks_t *benchmarks )
{
    unsigned int i;
    /* print again with only %lu where %llu not supported */
    printf( "\n=(%u*(2^32))+%u", (unsigned int)(benchmarks->tsc >> 32), (unsigned int)benchmarks->tsc );
    for( i = 0; i < benchmarks->size; i++ )
	printf( ",=(%u*(2^32))+%u", (unsigned int)(benchmarks->benchmarks[i].counter >> 32), (unsigned int)benchmarks->benchmarks[i].counter );

    printf( ",<- %%u only\n" );
}

extern inline void perfmon_print( benchmarks_t *benchmarks )
{
    perfmon_print_headers( benchmarks );
    perfmon_print_data( benchmarks );
}

extern inline benchmarks_t *
perfmon_arch_init( void )
{
    benchmarks_t *benchmarks;
    enum bm_type_t bmt;
    const char *msg;

    bmt = ia32_cpu_type();
    switch( bmt ) {
	case BMT_P4:
	    msg = "Pentium 4 detected, enabling P4 performance monitoring.\n";
	    benchmarks = &p4_benchmarks;
	    break;
	case BMT_K8:
	    msg = "AMD K8 detected, enabling K8 performance monitoring.\n";
	    benchmarks = &k8_benchmarks;
	    break;
	default:
	    benchmarks = (benchmarks_t *)0;
	    msg = "Unknown CPU type, performance monitoring disabled.\n";
    }
    printf( msg );

    return benchmarks;
}

#endif	/* __L4KA_RESOURCEMON__COMMON__IA32__PERFMON_H__ */

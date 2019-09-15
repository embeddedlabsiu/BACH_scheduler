#ifndef __STATS_H__
#define __STATS_H__

#include <cpuset.h>
#include <bitmask.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "utils.h"
#include "prfcntr_xeon.h"
#include "tsc.h"

#define NEHALEM_EP 			26
#define NEHALEM 			30
#define ATOM 				28
#define ATOM_2 				53
#define ATOM_CENTERTON 		54
#define ATOM_BAYTRAIL 		55
#define ATOM_AVOTON			77
#define ATOM_CHERRYTRAIL	76
#define ATOM_APOLLO_LAKE 	92
#define CLARKDALE 			37
#define WESTMERE_EP 		44
#define NEHALEM_EX 			46
#define WESTMERE_EX 		47
#define SANDY_BRIDGE 		42
#define JAKETOWN 			45
#define IVY_BRIDGE 			58
#define HASWELL 			60
#define HASWELL_ULT 		69
#define HASWELL_2 			70
#define IVYTOWN 			62
#define HASWELLX 			63
#define BROADWELL 			61
#define BROADWELL_XEON_E3 	71
#define BDX_DE 				86
#define SKL_UY 				78
#define BDX 				79
#define KNL 				87
#define SKL 				94

union CPUID_INFO
{
    int array[4];
    struct { unsigned int eax, ebx, ecx, edx; } reg;
};

typedef struct 
{
	__u32	UpscalingFactor;
	__u32	MaxRMID;
}L3Capabilities;

typedef struct 
{
	double	powerUnits;
	double	energyUnits;
	double	timeUnits;
}powerInfo;

typedef struct 
{
	__u64 prev_pkg_energy_counter;
	__u64 prev_pp0_energy_counter;
	__u64 prev_dram_energy_counter;
	__u64 pkg_energy_counter;
	__u64 pp0_energy_counter;
	__u64 dram_energy_counter;
}energyCounters;

typedef struct 
{
	int size;
	int fd[5];		// five channels, 4 in one MC and 1 in the other
	__u64 previousMCNormalReads;
	__u64 previousMCFullWrites;
	__u64 UncMCNormalReads;
	__u64 UncMCFullWrites;
}UNCMCPCI;

struct aff_counters {
	UNCMCPCI		MCinfo;
	L3Capabilities 	L3Cap;
	__u64			TCCActivation;
	__u64			TCCOffset;
	powerInfo		raplUnits;
	energyCounters	energy_counters;
	int 			cpus_nr;
	int 			*cpus;
	prfcnt_t 		*prfcnt;
	__u64 			**events_tots;
	FILE 			*cntoutF;
};
typedef struct aff_counters aff_counters_t;

struct aff_stats {
	tsc_t waiting;
	tsc_t total_running;
	tsc_t *running;
	__u64 **events_tmp;
	__u64 **events;
	__u64 *L3OccupancyMetric;
	__u64 *DTSMetric;
	__u64 *PLTMetric;
	__u64 *tmp;
	int cpu;
	int events_nr;
	aff_counters_t *cntrs;
	struct bitmask *tmp_bm;
	enum {
		STATS_ON,
		STATS_OFF
	} stats_state;
};
typedef struct aff_stats aff_stats_t;

aff_counters_t * ss_aff_counters_alloc(int cpus_nr);
void ss_aff_counters_free(aff_counters_t *cntrs); 
aff_counters_t * ss_aff_counters_init(int cpus_nr, int * cpus);
void ss_aff_counter_start(aff_counters_t *cntrs, int cpu); 
void ss_aff_stop_counter(aff_counters_t *cntrs, int cpu);
void ss_aff_read_counter(aff_counters_t *cntrs, int cpu);
void ss_aff_counters_start(aff_counters_t *cntrs); 
void ss_aff_stop_counters(aff_counters_t *cntrs);
void ss_aff_read_counters(aff_counters_t *cntrs);
void ss_aff_zero_cntrs(aff_counters_t *cntrs);
void ss_change_counter(aff_counters_t *cntrs, int event_nr, int event);
void ss_close_counters(aff_counters_t *);

aff_stats_t * ss_aff_stats_alloc(aff_counters_t *cntrs);
void ss_aff_stats_free(aff_stats_t *stats);
aff_stats_t * ss_aff_stats_init(aff_counters_t *cntrs);
void ss_aff_stats_start(aff_stats_t *stats, struct cpuset *cp);
void ss_aff_stats_pause(aff_stats_t *stats, struct cpuset *cp);
void ss_aff_stats_read(aff_stats_t *stats, struct cpuset *cp);
void ss_aff_zero_stats(aff_stats_t *stats);
void ss_aff_stats_shutdown(aff_stats_t *stats, struct cpuset *cp);

__u64 ss_get_counter(aff_stats_t *stats, int counter,  char per_pack);
__u64 get_total_instr(aff_stats_t *stats);
__u64 get_total_cycles(aff_stats_t *stats);

void stopMCCounters (aff_counters_t *);
void startMCCounters (aff_counters_t *);
void Pciclose(aff_counters_t *);

void print_cpu_characteristics(void );
void InitRMID (aff_counters_t *);
void InitPower (aff_counters_t *);
void startPower (aff_counters_t *);
void stopPower (aff_counters_t *);
double getEnergyCounters (aff_counters_t *, char);
#endif /* __STATS_H__ */

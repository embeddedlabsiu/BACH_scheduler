#ifndef __PRFCNT_SANDY_H__
#define __PRFCNT_SANDY_H__

#define prfcnt_readstats prfcnt_readstats_rdmsr
#define NUM_COUNTERS 	11
#define OFFSET_PMC 		3

#include <math.h>
#include "prfcnt_common.h"
#include "prfcntr_xeon_types.h"
#include "prfcntr_xeon_events.h"

char overf;

static void monitor_init(prfcnt_t *handle){
	__u64 flags 	= 0x0ULL;
	int msr_fd	= handle->msr_fd;

	/* Fixed Counters: instructions retired, cycles unhalted core */
	handle->counters[PMC0].configRegister  = MSR_PERF_FIXED_CTR_CTRL;
	handle->counters[PMC0].counterRegister = MSR_PERF_FIXED_CTR0;
	handle->counters[PMC0].type            = FIXED;
	handle->counters[PMC0].init            = 1;
	handle->counters[PMC1].configRegister  = MSR_PERF_FIXED_CTR_CTRL;
	handle->counters[PMC1].counterRegister = MSR_PERF_FIXED_CTR1;
	handle->counters[PMC1].type            = FIXED;
	handle->counters[PMC1].init            = 1;
	handle->counters[PMC2].configRegister  = MSR_PERF_FIXED_CTR_CTRL;
	handle->counters[PMC2].counterRegister = MSR_PERF_FIXED_CTR2;
	handle->counters[PMC2].type            = FIXED;
	handle->counters[PMC2].init            = 1;
	
	/* PMC Counters: 4 48bit wide */
	handle->counters[PMC3].configRegister  = MSR_PERFEVTSEL0;
	handle->counters[PMC3].counterRegister = MSR_PMC0;
	handle->counters[PMC3].type            = PMC;
	handle->counters[PMC3].init            = 0;
	handle->counters[PMC4].configRegister  = MSR_PERFEVTSEL1;
	handle->counters[PMC4].counterRegister = MSR_PMC1;
	handle->counters[PMC4].type            = PMC;
	handle->counters[PMC4].init            = 0;
	handle->counters[PMC5].configRegister  = MSR_PERFEVTSEL2;
	handle->counters[PMC5].counterRegister = MSR_PMC2;
	handle->counters[PMC5].type            = PMC;
	handle->counters[PMC5].init            = 0;
	handle->counters[PMC6].configRegister  = MSR_PERFEVTSEL3;
	handle->counters[PMC6].counterRegister = MSR_PMC3;
	handle->counters[PMC6].type            = PMC;
	handle->counters[PMC6].init            = 0;

	/* Zero the MSRs */
	prfcnt_wrmsr(msr_fd, MSR_PERF_FIXED_CTR_CTRL,  0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL0,          0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL1,          0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL2,          0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL3,          0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PMC0,                 0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PMC1,                 0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PMC2,                 0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PMC3,                 0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERF_FIXED_CTR0,      0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERF_FIXED_CTR1,      0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERF_FIXED_CTR2,      0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERF_GLOBAL_CTRL,     0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PERF_GLOBAL_OVF_CTRL, 0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_PEBS_ENABLE,          0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_OFFCORE_RSP0, 	       0x0ULL);
	prfcnt_wrmsr(msr_fd, MSR_OFFCORE_RSP1, 	       0x0ULL);

	/* Initialize bit fields of the MSR_PERF_FIXED_CTR_CTRL for FIXED COUNTERS */
	prfcnt_wrmsr(msr_fd, MSR_PERF_FIXED_CTR_CTRL,  0x333ULL);
	
	flags |= (1 << 22);  /* enable flag */
	flags |= (3 << 16);  /* user flag */
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL0,        flags);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL1,        flags);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL2,        flags);
	prfcnt_wrmsr(msr_fd, MSR_PERFEVTSEL3,        flags);
}

static inline void prfcnt_init(prfcnt_t *handle, int cpu, unsigned long flags)
{
	int i;

	__u64 rflags;
	handle->cpu       = cpu;
	handle->flags     = flags;
	handle->events_nr = sizeof(__evnts_selected) / sizeof(int); 
	handle->events    = malloc(handle->events_nr * sizeof(prfcnt_event_t *));

	if (handle->L3COMetricAvailable) {
		handle->L3OccupancyInfo.configRegister 		= IA32_QM_CTR;
		handle->L3OccupancyInfo.counterRegister		= 0;
	}

	if (handle->DTSAvailable) {
		handle->DTSInfo.configRegister 		= MSR_IA32_THERM_STATUS;
		handle->DTSInfo.counterRegister		= 0;
	}

	if (handle->PLTAvailable) {
		handle->PLTInfo.configRegister 		= MSR_IA32_PACKAGE_THERM_STATUS;
		handle->PLTInfo.counterRegister		= 0;
	}

	if ( !handle->events ){ perror("prfcnt_init"); 	exit(1); }

	__prfcnt_init(cpu, &handle->msr_fd);

	monitor_init(handle);

	for (i=0; i < handle->events_nr; i++){ 
		if ( __evnts_selected[i] < 0) continue;						/* No event selected for this counter */
		if ( FIXED_EVENTS_OFFSET <= __evnts_selected[i]) continue;	/* Fixed event will be initialized anyhow */

		prfcnt_event_t      *event;
		event             = &__evnts[__evnts_selected[i]];
		handle->events[i] = event;

		switch( event->type ) {
			case PMC:
				handle->counters[i].init = 1;
				prfcnt_rdmsr(handle->msr_fd, handle->counters[i].configRegister, &rflags);
				rflags &= ~(0xFFFFU);
				rflags |= ( event->umask << 8); 
				rflags |=   event->evnt;
				if (event->cfgBits != 0) { /* set custom cfg and cmask */
					rflags &= ~(0xFFFFU<<16);  /* clear upper 16bits */
					rflags |= ((event->cmask<<8) + event->cfgBits)<<16;
				}


				prfcnt_wrmsr(handle->msr_fd, handle->counters[i].configRegister, rflags);

				if ((event->evnt == 0xB7) && (event->umask == 0x01)) {
					prfcnt_rdmsr(handle->msr_fd, MSR_OFFCORE_RSP0, &event->off_mask.value);
					event->off_mask.fields.dmnd_data_rd = 1;
					event->off_mask.fields.dmnd_rfo = 1;
					event->off_mask.fields.dmnd_ifetch = 0;
					event->off_mask.fields.corewb = 0;
					event->off_mask.fields.pf_data_rd = 1;
					event->off_mask.fields.pf_rfo = 1;
					event->off_mask.fields.pf_ifetch = 0;
					event->off_mask.fields.pf_l3_data_rd = 1;
					event->off_mask.fields.pf_l3_rfo = 1;
					event->off_mask.fields.pf_l3_code_rd = 0;
					event->off_mask.fields.split_lock_uc_lock = 0;
					event->off_mask.fields.strm_st = 0;
					event->off_mask.fields.other = 0;
					event->off_mask.fields.any = 0;
					event->off_mask.fields.no_supp = 0;
					event->off_mask.fields.l3_hitm = 0;
					event->off_mask.fields.l3_hite = 0;
					event->off_mask.fields.l3_hits = 0;
					event->off_mask.fields.l3_hitf = 0;
					event->off_mask.fields.local = 1;
					event->off_mask.fields.l3_miss_remote_hop0 = 0;
					event->off_mask.fields.l3_miss_remote_hop1 = 0;
					event->off_mask.fields.l3_miss_remote_hop2p = 0;
					event->off_mask.fields.snp_none = 0;
					event->off_mask.fields.snp_not_needed = 0;
					event->off_mask.fields.snp_miss = 1;
					event->off_mask.fields.snp_no_fwd = 0;
					event->off_mask.fields.snp_fwd = 0;
					event->off_mask.fields.hitm = 0;
					event->off_mask.fields.non_dram = 0;

					prfcnt_wrmsr(handle->msr_fd, MSR_OFFCORE_RSP0, event->off_mask.value);
				}
				if ((event->evnt == 0xBB) && (event->umask == 0x01)) {
					prfcnt_wrmsr(handle->msr_fd, MSR_OFFCORE_RSP1, event->off_mask.value);
				}
			default:
				break;
		}
	}
}

static inline void prfcnt_shut(prfcnt_t *handle)
{
	__prfcnt_shut(handle->msr_fd);
}

static inline void prfcnt_start(prfcnt_t *handle)
{
	__u64 flags;

	prfcnt_rdmsr(handle->msr_fd, MSR_PERF_GLOBAL_CTRL, &flags);

	flags |= 0x000000070000000F;

    prfcnt_wrmsr(handle->msr_fd, MSR_PERF_GLOBAL_CTRL, flags);
}

static inline void prfcnt_pause(prfcnt_t *handle)
{
	__u64 flags;

	prfcnt_rdmsr(handle->msr_fd, MSR_PERF_GLOBAL_CTRL, &flags);

	flags &= 0xFFFFFFF8FFFFFFF0;

	prfcnt_wrmsr(handle->msr_fd, MSR_PERF_GLOBAL_CTRL, flags);

	overf = 0;
	prfcnt_rdmsr(handle->msr_fd, MSR_PERF_GLOBAL_STATUS, &flags);	
	if((flags & 0xfULL)) 
	{
		overf = 1;
		printf("\nOVERFLOW1! TAKE ACTION!\n");
	}
}

static inline void prfcnt_read_cnts_rdmsr(prfcnt_t *handle)
{
    int i;
	__u64 data;

	for (i = 0; i < handle->events_nr; i++)	{
		prfcnt_rdmsr(handle->msr_fd, handle->counters[i].counterRegister, &data);
		handle->counters[i].counterData = data;
	}

	if (handle->L3COMetricAvailable) {
		prfcnt_rdmsr(handle->msr_fd, handle->L3OccupancyInfo.configRegister, &data);
		if ((data>>63) & 0x01) printf("IA32_QM_CTR: No valid data to report\n");
		else if ((data>>62) & 0x01) printf("IA32_QM_CTR: Monitored data for the RMID is not available\n");
		else handle->L3OccupancyInfo.counterRegister = (data<<2)>>2;
	}

	if (handle->DTSAvailable) {
		prfcnt_rdmsr(handle->msr_fd, handle->DTSInfo.configRegister, &data);
		if (data & (1<<31)) handle->DTSInfo.counterRegister = (data>>16) & 0x7fULL;
		else printf("MSR_IA32_THERM_STATUS: No valid data to report\n");
	}

	if (handle->PLTAvailable) {
		prfcnt_rdmsr(handle->msr_fd, handle->PLTInfo.configRegister, &data);
		handle->PLTInfo.counterRegister = (data>>16) & 0x7fULL;
	}
}

static inline void prfcnt_readstats_rdmsr(prfcnt_t *handle, __u64 *stats){
	int i;

	for (i = 0; i < handle->events_nr; i++){
		stats[i] = handle->counters[i].counterData;
	}
}


static inline void prfcnt_zerocounters(prfcnt_t *handle){
	int i;

	for (i = 0; i < handle->events_nr; i++) prfcnt_wrmsr(handle->msr_fd, handle->counters[i].counterRegister, 0x00ULL);
}



#endif

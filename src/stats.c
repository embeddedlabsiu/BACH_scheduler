#include "stats.h"
#include "aff-executor.h"

void handlePrefetchers (aff_counters_t *cntrs, char mode)
{
    int         i;
    __u64       data;
    prfcnt_t    *handle;


    for(i = 0; i < cntrs->cpus_nr; i++)
    {
        handle = &cntrs->prfcnt[i];

        prfcnt_rdmsr(handle->msr_fd, MSR_IA32_DISABLE_PREF, &data);

        if (mode == 1) data &= 0xfffffffffffffff0;
        else if (mode == 0) data |= 0xfULL;

        prfcnt_wrmsr(handle->msr_fd, MSR_IA32_DISABLE_PREF, data);
    }
}

int Pciopen(char *bus, __u32 device, __u32 function)
{
    const char *str = "/proc/bus/pci/";
    char path[22];
    int fd=0;

    sprintf(path, "%s%s/%d.%d", str, bus, device, function);

    path[21] = '\0';

    fd = open(path, O_RDWR);

    if (fd < 0) {
        perror("open()");
        exit(-1);
    }

    return fd;
}

int Pciread32(int fd, __u64 offset, __u32 * value)
{
    ssize_t result;
    result = pread(fd, (void *)value, sizeof(__u32), offset);
    return result;
}

int Pciwrite32(int fd, __u64 offset, __u32 value)
{
    // std::cout << "write32 ImcHandle\n";
    ssize_t result;
    result = pwrite(fd, (const void *)&value, sizeof(__u32), offset);
    return result;
}

int Pciread64(int fd, __u64 offset, __u64 * value)
{
    // std::cout << "read64 ImcHandle\n";
    int res = pread(fd, (void *)value, sizeof(__u64), offset);
    if(res != sizeof(__u64))
    {
        printf (" ERROR: pread from %d with offset 0x%llx returned %d bytes\n", fd, offset, res);
    }
    return res;
}

void Pciclose(aff_counters_t *cntrs)
{
    UNCMCPCI *handle;
    int i;
    handle = &cntrs->MCinfo;

    for (i = 0; i < handle->size; i++)
    {
        if (handle->fd[i]) close(handle->fd[i]);
    }
}

void Pciopensocket (UNCMCPCI * info, char * socket)
{
    info->fd[0] = Pciopen(socket, MC0_CHANNEL0_DEV_ADDR, MC0_CHANNEL0_FUNC_ADDR);
    info->fd[1] = Pciopen(socket, MC0_CHANNEL1_DEV_ADDR, MC0_CHANNEL1_FUNC_ADDR);
    info->fd[2] = Pciopen(socket, MC0_CHANNEL2_DEV_ADDR, MC0_CHANNEL0_FUNC_ADDR);
    info->fd[3] = Pciopen(socket, MC0_CHANNEL3_DEV_ADDR, MC0_CHANNEL1_FUNC_ADDR);
    info->fd[4] = Pciopen(socket, MC1_CHANNEL0_DEV_ADDR, MC0_CHANNEL0_FUNC_ADDR);
}

void Pciprogram (UNCMCPCI info)
{
    __u32 MCCntConfig[4] = {0, 0, 0, 0};
    __u32 i;

    MCCntConfig[0] = MC_CH_PCI_PMON_CTL_EVENT(0x04) + MC_CH_PCI_PMON_CTL_UMASK(3);  // monitor reads on counter 0: CAS_COUNT.RD
    MCCntConfig[1] = MC_CH_PCI_PMON_CTL_EVENT(0x04) + MC_CH_PCI_PMON_CTL_UMASK(12); // monitor writes on counter 1: CAS_COUNT.WR

    for (i = 0; i < info.size; ++i)
    {
        // imc PMU
        // freeze enable
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR, MC_CH_PCI_PMON_BOX_CTL_FRZ_EN);
        // freeze
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR, MC_CH_PCI_PMON_BOX_CTL_FRZ_EN + MC_CH_PCI_PMON_BOX_CTL_FRZ);

        // enable fixed counter (DRAM clocks)
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_FIXED_CTL_ADDR, MC_CH_PCI_PMON_FIXED_CTL_EN);

        // reset it
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_FIXED_CTL_ADDR, MC_CH_PCI_PMON_FIXED_CTL_EN + MC_CH_PCI_PMON_FIXED_CTL_RST);

        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL0_ADDR, MC_CH_PCI_PMON_CTL_EN);
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL0_ADDR, MC_CH_PCI_PMON_CTL_EN + MCCntConfig[0]);

        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL1_ADDR, MC_CH_PCI_PMON_CTL_EN);
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL1_ADDR, MC_CH_PCI_PMON_CTL_EN + MCCntConfig[1]);

        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL2_ADDR, MC_CH_PCI_PMON_CTL_EN);
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL2_ADDR, MC_CH_PCI_PMON_CTL_EN + MCCntConfig[2]);

        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL3_ADDR, MC_CH_PCI_PMON_CTL_EN);
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_CTL3_ADDR, MC_CH_PCI_PMON_CTL_EN + MCCntConfig[3]);

        // reset counters values
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR, MC_CH_PCI_PMON_BOX_CTL_FRZ_EN + MC_CH_PCI_PMON_BOX_CTL_FRZ + MC_CH_PCI_PMON_BOX_CTL_RST_COUNTERS);

        // unfreeze counters
        Pciwrite32(info.fd[i], XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR, MC_CH_PCI_PMON_BOX_CTL_FRZ_EN);
    }
}

__u64 getImcReads(UNCMCPCI handle)
{
    __u64 result = 0;
    __u64 value = 0;
    __u32 i;

    for (i = 0; i < handle.size ; ++i) {
        value = 0;
        Pciread64(handle.fd[i], XPF_MC_CH_PCI_PMON_CTR0_ADDR, &value);
        result += value;
    }

    return result;
}

__u64 getImcWrites(UNCMCPCI handle)
{

    __u64 result = 0;
    __u64 value = 0;
    __u32 i;

    for (i = 0; i < handle.size; ++i) {
        value = 0;
        Pciread64(handle.fd[i], XPF_MC_CH_PCI_PMON_CTR1_ADDR, &value);
        result += value;
    }

    return result;
}

void freezeCounters(UNCMCPCI handle)
{
    size_t i;

    for (i = 0; i < handle.size; ++i) {
        Pciwrite32(handle.fd[i], XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR, MC_CH_PCI_PMON_BOX_CTL_FRZ_EN + MC_CH_PCI_PMON_BOX_CTL_FRZ);
    }
}


void unfreezeCounters(UNCMCPCI handle)
{
    size_t i;

    for (i = 0; i < handle.size; ++i) {
        Pciwrite32(handle.fd[i], XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR, MC_CH_PCI_PMON_BOX_CTL_FRZ_EN);
    }
}

void startMCCounters (aff_counters_t *cntrs)
{
    freezeCounters(cntrs->MCinfo);
    cntrs->MCinfo.previousMCNormalReads = getImcReads(cntrs->MCinfo);
    cntrs->MCinfo.previousMCFullWrites = getImcWrites(cntrs->MCinfo);
    unfreezeCounters(cntrs->MCinfo);
}

void stopMCCounters (aff_counters_t *cntrs)
{
    __u64 currentReads, currentWrites;

    freezeCounters(cntrs->MCinfo);
    currentReads = getImcReads(cntrs->MCinfo);
    currentWrites = getImcWrites(cntrs->MCinfo);
    unfreezeCounters(cntrs->MCinfo);

    cntrs->MCinfo.UncMCNormalReads = currentReads - cntrs->MCinfo.previousMCNormalReads;
    cntrs->MCinfo.UncMCFullWrites = currentWrites - cntrs->MCinfo.previousMCFullWrites;
}
/*
 * allocate a aff_counters_t struct 
 * and memory for performance counters
 */       
aff_counters_t * ss_aff_counters_alloc(int cpus_nr)
{
        aff_counters_t	*ret;

        if ( (ret 	  = malloc(sizeof(*ret)))               == NULL) perrdie("malloc", "");
        if ( (ret->cpus   = malloc(cpus_nr * sizeof(int)))      == NULL) perrdie("malloc", "");
        if ( (ret->prfcnt = malloc(cpus_nr * sizeof(prfcnt_t))) == NULL) perrdie("malloc", "");
        return ret; 
}   

void ss_aff_counters_free(aff_counters_t *cntrs)
{
        free(cntrs->cpus);
        free(cntrs->prfcnt);
        free(cntrs);
}
//-----------------------------------------------------------------------------------

/*
 * initialize performance counters. put one performance counter counting events in every cpu.
 */
aff_counters_t * ss_aff_counters_init(int cpus_nr, int * cpus)
{
    int i;
    aff_counters_t *cntrs;
    __u64 data;

    fprintf(ExecSt.fsched, "initializing performance counting in cpus: [");
	for(i = 0; i < cpus_nr - 1; i++) fprintf(ExecSt.fsched, "%d, ", cpus[i]);
	fprintf(ExecSt.fsched, "%d]\n", cpus[i]);	

    cntrs = ss_aff_counters_alloc(cpus_nr);
    cntrs->cpus_nr = cpus_nr;
    for (i=0; i < cntrs->cpus_nr; ++i) {
        cntrs->cpus[i] = cpus[i];
        cntrs->prfcnt[i].L3COMetricAvailable = (ExecSt.CMTAvailable) ? 1 : 0;
        cntrs->prfcnt[i].DTSAvailable = (ExecSt.DTSAvailable) ? 1 : 0;
        cntrs->prfcnt[i].PLTAvailable = (ExecSt.PLTAvailable) ? 1 : 0;
        prfcnt_init(&cntrs->prfcnt[i], cntrs->cpus[i], 0x00ULL);
    }

    prfcnt_rdmsr(cntrs->prfcnt[0].msr_fd, MSR_TEMPERATURE_TARGET, &data);
    cntrs->TCCActivation = ((data>>16) & 0xffULL);
    cntrs->TCCOffset = ((data>>24) & 0xfULL);

    handlePrefetchers(cntrs, 1);
    cntrs->MCinfo.size = 5;
    cntrs->MCinfo.UncMCFullWrites = 0;
    cntrs->MCinfo.UncMCNormalReads = 0;
    Pciopensocket (&cntrs->MCinfo, SOCKET0_BUS);
    Pciprogram (cntrs->MCinfo);
	
	return cntrs;
}

void ss_aff_counter_start(aff_counters_t *cntrs, int cpu)
{
    int i;
    for (i = 0; i < cntrs->cpus_nr; ++i) {
        if (cntrs->cpus[i] == cpu) prfcnt_start(&cntrs->prfcnt[i]);
    }
}

void ss_aff_stop_counter(aff_counters_t *cntrs, int cpu)
{
    int i;
    for (i = 0; i < cntrs->cpus_nr; ++i) {
        if (cntrs->cpus[i] == cpu) prfcnt_pause(&cntrs->prfcnt[i]);
    }
}

void ss_aff_read_counter(aff_counters_t *cntrs, int cpu)
{
    int i;
    for (i = 0; i < cntrs->cpus_nr; ++i) {
        if (cntrs->cpus[i] == cpu) prfcnt_read_cnts_rdmsr(&cntrs->prfcnt[i]);
    }
}

/*********************************************************************************/

void ss_aff_counters_start(aff_counters_t *cntrs)
{
        int i;
        for (i = 0; i < cntrs->cpus_nr; ++i) prfcnt_start(&cntrs->prfcnt[i]);
}

void ss_aff_stop_counters(aff_counters_t *cntrs)
{
    int i;
    for (i = 0; i < cntrs->cpus_nr; ++i) prfcnt_pause(&cntrs->prfcnt[i]);
}

void ss_aff_read_counters(aff_counters_t *cntrs)
{
    int i;
    for (i = 0; i < cntrs->cpus_nr; ++i) prfcnt_read_cnts_rdmsr(&cntrs->prfcnt[i]);
}

void ss_aff_zero_cntrs(aff_counters_t *cntrs)
{
    int i; 
    for (i=0; i<cntrs->cpus_nr; ++i) prfcnt_zerocounters(&cntrs->prfcnt[i]);
}

void ss_close_counters(aff_counters_t *cntrs)
{
    int i;  
    for (i = 0; i < cntrs->cpus_nr; ++i) prfcnt_shut(&cntrs->prfcnt[i]);
}

void ss_change_counter(aff_counters_t *cntrs, int event_nr, int event)
{
    int i;  
	__evnts_selected[event_nr] = event;
    for (i=0; i < cntrs->cpus_nr; ++i) prfcnt_init(&cntrs->prfcnt[i], cntrs->cpus[i], 0x00ULL);
}

/**********************************************************************************/

aff_stats_t * ss_aff_stats_alloc(aff_counters_t *cntrs)
{
    aff_stats_t *ret;
    int i;

    if ((ret = malloc(sizeof(*ret))) == NULL) perrdie("malloc", "");
    ret->events_nr = cntrs->prfcnt[0].events_nr;
    ret->cntrs = cntrs;

    if ((ret->running = malloc(cntrs->cpus_nr * sizeof(tsc_t))) == NULL) perrdie("malloc", "");
    if ((ret->events_tmp = malloc(cntrs->cpus_nr * sizeof(__u64 *))) == NULL) perrdie("malloc", "");
    if ((ret->events = malloc(cntrs->cpus_nr * sizeof(__u64 *))) == NULL) perrdie("malloc", "");
    if ((ret->tmp = malloc(ret->events_nr * sizeof(__u64))) == NULL) perrdie("malloc", "");
    if ((ret->tmp_bm = bitmask_alloc(cpuset_cpus_nbits())) == NULL) perrdie("bitmask_malloc", "");
    if (cntrs->prfcnt[0].L3COMetricAvailable) {
        if ((ret->L3OccupancyMetric = malloc(cntrs->cpus_nr * sizeof(__u64))) == NULL) perrdie("malloc", "");
    } else ret->L3OccupancyMetric = NULL;

    if (cntrs->prfcnt[0].DTSAvailable) {
        if ((ret->DTSMetric = malloc(cntrs->cpus_nr * sizeof(__u64))) == NULL) perrdie("malloc", "");
    } else ret->DTSMetric = NULL;

    if (cntrs->prfcnt[0].PLTAvailable) {
        if ((ret->PLTMetric = malloc(cntrs->cpus_nr * sizeof(__u64))) == NULL) perrdie("malloc", "");
    } else ret->PLTMetric = NULL;

    for (i=0; i<cntrs->cpus_nr; ++i) {
            if ((ret->events[i] = malloc(ret->events_nr * sizeof(__u64))) == NULL ) perrdie("malloc", "");
            if ( (ret->events_tmp[i] = malloc(ret->events_nr * sizeof(__u64))) == NULL ) perrdie("malloc", "");
    }

    return ret;
}

void ss_aff_stats_free(aff_stats_t *stats)
{
    int i;

    for(i=0; i<stats->cntrs->cpus_nr; ++i) {
            free(stats->events[i]);
            free(stats->events_tmp[i]);
    }

    if (stats->L3OccupancyMetric != NULL) free(stats->L3OccupancyMetric);
    free(stats->running);
    free(stats->tmp);
    free(stats->events);
    free(stats->events_tmp);
    bitmask_free(stats->tmp_bm);
    free(stats);
}
//-----------------------------------------------------------------------------------

aff_stats_t * ss_aff_stats_init(aff_counters_t *cntrs)
{
    int i,j;
    aff_stats_t *stats;

    stats = ss_aff_stats_alloc(cntrs);

    for (i=0; i < cntrs->cpus_nr; ++i) {
        for (j = 0; j < stats->events_nr; ++j) {
                stats->events[i][j]     = 0;
                stats->events_tmp[i][j] = 0;
        }
        tsc_init(&stats->running[i]);
        if (stats->L3OccupancyMetric != NULL) stats->L3OccupancyMetric[i] = 0;
        if (stats->DTSMetric != NULL) stats->DTSMetric[i] = 0;
        if (stats->PLTMetric != NULL) stats->PLTMetric[i] = 0;
    }
    tsc_init(&stats->total_running);
    tsc_init(&stats->waiting);
    tsc_start(&stats->waiting);
    stats->stats_state = STATS_OFF;

    return stats;
}

void ss_aff_stats_start(aff_stats_t *stats, struct cpuset *cp){
        int ret, i;
        struct bitmask *bm = stats->tmp_bm;
        aff_counters_t *cntrs = stats->cntrs;

        ret = cpuset_getcpus(cp, bm);
        if (ret < 0) die("cpuset_getcpus");

        tsc_pause(&stats->waiting);
        tsc_start(&stats->total_running);

        for (i = 0; i < cntrs->cpus_nr; ++i) {
                if(bitmask_isbitset(bm, cntrs->cpus[i])) {
        		tsc_start(&stats->running[i]);
                }
        }
#ifdef CPU_SANDY
	bw_cntr_read_stats(stats->bw0, stats->bw_old_values[0]);
	bw_cntr_read_stats(stats->bw1, stats->bw_old_values[1]);
	bw_cntr_read_stats(stats->bw2, stats->bw_old_values[2]);
	bw_cntr_read_stats(stats->bw3, stats->bw_old_values[3]);
#endif
	stats->stats_state = STATS_ON;
}

void ss_aff_stats_pause(aff_stats_t *stats, struct cpuset *cp){
        int i, ret;
        struct bitmask *bm = stats->tmp_bm;
        aff_counters_t *cntrs = stats->cntrs;

        ret = cpuset_getcpus(cp, bm);
        if (ret < 0) die("cpuset_getcpus");

        tsc_pause(&stats->total_running);
        tsc_start(&stats->waiting);
        for (i = 0; i < cntrs->cpus_nr; ++i) 
        {
                if (bitmask_isbitset(bm, cntrs->cpus[i])) {
        		tsc_pause(&stats->running[i]);
                }
        }
#ifdef CPU_SANDY
	bw_cntr_read_stats(stats->bw0, stats->bw_curr_values[0]);
	bw_cntr_read_stats(stats->bw1, stats->bw_curr_values[1]);
	bw_cntr_read_stats(stats->bw2, stats->bw_curr_values[2]);
	bw_cntr_read_stats(stats->bw3, stats->bw_curr_values[3]);
#endif
        stats->stats_state = STATS_OFF;
}

void ss_aff_stats_read(aff_stats_t *stats, struct cpuset *cp){
	int i, j, ret;  
	struct bitmask *bm = stats->tmp_bm;
	aff_counters_t *cntrs = stats->cntrs;
	
	ret = cpuset_getcpus(cp, bm);
	if (ret < 0) die("cpuset_getcpus"); 
	
	for (i = 0; i < cntrs->cpus_nr; ++i) {
		if (bitmask_isbitset(bm, cntrs->cpus[i])) {
			stats->cpu = cntrs->cpus[i];
			for (j = 0; j < stats->events_nr; ++j){ 
    				stats->events[i][j]      = cntrs->prfcnt[i].counters[j].counterData;
    				stats->events_tmp[i][j] += cntrs->prfcnt[i].counters[j].counterData;
			}

            if (cntrs->prfcnt[i].L3COMetricAvailable) {
                stats->L3OccupancyMetric[i] = (cntrs->prfcnt[i].L3OccupancyInfo.counterRegister * cntrs->L3Cap.UpscalingFactor);
            }

            if (cntrs->prfcnt[i].DTSAvailable) {
                stats->DTSMetric[i] = cntrs->TCCActivation - cntrs->prfcnt[i].DTSInfo.counterRegister;
            } 

            if (cntrs->prfcnt[i].PLTAvailable) {
                stats->PLTMetric[i] = cntrs->TCCActivation - cntrs->prfcnt[i].PLTInfo.counterRegister;
            } 
		}
	}
}

void ss_aff_zero_stats(aff_stats_t *stats){
	int i,j;
	aff_counters_t *cntrs = stats->cntrs;

	for (i=0; i<cntrs->cpus_nr; i++) {
		for (j=0; j<stats->events_nr; j++) {
			stats->events[i][j] = 0;
		}

        if (stats->L3OccupancyMetric != NULL) stats->L3OccupancyMetric[i] = 0;
        if (stats->DTSMetric != NULL) stats->DTSMetric[i] = 0;
        if (stats->PLTMetric != NULL) stats->PLTMetric[i] = 0;
	}
}

void ss_aff_stats_shutdown(aff_stats_t *stats, struct cpuset *cp) {
        if (stats->stats_state == STATS_ON)
                ss_aff_stats_pause(stats, cp);
        else
                tsc_pause(&stats->waiting);
}

__u64 get_total_instr(aff_stats_t *stats){
	int threads;
	__u64 value = 0;
	aff_counters_t *cntrs = stats->cntrs;

	for (threads = 0; threads < cntrs->cpus_nr; ++threads){
		value += stats->events_tmp[threads][0];
		}
	return value;
}

__u64 get_total_cycles(aff_stats_t *stats){
    int threads;
    __u64 value = 0;
    aff_counters_t *cntrs = stats->cntrs;

    for (threads = 0; threads < cntrs->cpus_nr; ++threads){
        value += stats->events_tmp[threads][1];
        }
    return value;
}

__u64 ss_get_counter(aff_stats_t *stats, int counter, char type)
{
    int            threads;
    __u64          value=0, ticks;
    aff_counters_t *cntrs = stats->cntrs;

	for (threads = 0; threads < cntrs->cpus_nr; ++threads) {
		ticks = tsc_getticks(&stats->running[threads]);
		if (ticks != 0) {
            switch (type) {
                case PROG_EVENT: 
                    value += stats->events[threads][counter];
                    break;
                case TEMP_EVENT:
                    if (counter == 0)
                    {
                        if (stats->DTSMetric != NULL) value += stats->DTSMetric[threads];
                    }
                    else
                    {
                        if (stats->PLTMetric != NULL) value += stats->PLTMetric[threads];
                    }
                    break;
                case RMID_EVENT:
                    if (stats->L3OccupancyMetric != NULL) value += stats->L3OccupancyMetric[threads];
            }
		}
	}
	return value;
}

const char * print_cpu_model(int cpu_model)
{
    switch(cpu_model)
    {
        case NEHALEM_EP:
        case NEHALEM:
            return "Nehalem/Nehalem-EP";
        case ATOM:
            return "Atom(tm)";
        case CLARKDALE:
            return "Westmere/Clarkdale";
        case WESTMERE_EP:
            return "Westmere-EP";
        case NEHALEM_EX:
            return "Nehalem-EX";
        case WESTMERE_EX:
            return "Westmere-EX";
        case SANDY_BRIDGE:
            return "Sandy Bridge";
        case JAKETOWN:
            return "Sandy Bridge-EP/Jaketown";
        case IVYTOWN:
            return "Ivy Bridge-EP/EN/EX/Ivytown";
        case HASWELLX:
            return "Haswell-EP/EN/EX";
        case BDX_DE:
            return "Broadwell-DE";
        case BDX:
            return "Broadwell-EP/EX";
        case KNL:
            return "Knights Landing";
        case IVY_BRIDGE:
            return "Ivy Bridge";
        case HASWELL:
            return "Haswell";
        case BROADWELL:
            return "Broadwell";
        case SKL:
            return "Skylake";
    }

    return "unknown";
}

void cpuid (unsigned int leaf, union CPUID_INFO *info)
{
    __asm__ volatile ("cpuid" : "=a" (info->reg.eax), "=b" (info->reg.ebx), "=c" (info->reg.ecx), "=d" (info->reg.edx) : "a" (leaf));
}

void cpuid_leaf (unsigned int leaf, unsigned int subleaf, union CPUID_INFO *info)
{
    __asm__ volatile ("cpuid" : "=a" (info->reg.eax), "=b" (info->reg.ebx), "=c" (info->reg.ecx), "=d" (info->reg.edx) : "a" (leaf), "c" (subleaf));
}

void print_cpu_characteristics()
{
    union {
        char cbuf[16];
        int ibuf[16/sizeof(int)];
    }buf;

    union CPUID_INFO info, *info1;
    unsigned int cpu_model, msr, hyper, perf_version, max_gen_counter_num, gen_counter_width;
    unsigned int fixed_counter_width = 0, max_fixed_counter_num = 0;
    int max_leaf;
    char str[49];

    printf("\n#############################################################################################\n");
    printf("#                                    CPU CHARACTERISTICS                                    #\n");
    printf("#############################################################################################\n");

    cpuid(0, &info);
    memset(buf.cbuf, 0, 16);
    memset(str, 0, 48);
    max_leaf = info.array[0];

    //GenuineIntel

    buf.ibuf[0] = info.array[1];
    buf.ibuf[1] = info.array[3];
    buf.ibuf[2] = info.array[2];

    printf("# Vendor: \t\t\t\t\t%s\n", buf.cbuf);

    //Processor's characteristics

    cpuid(0x80000000, &info);

    if (info.array[0] < 0x80000004){
        printf("Processor string not supported\n");
        exit(-1);
    } else {
        info1 = (union CPUID_INFO *) str;

        cpuid(0x80000002, info1);
        ++info1;
        cpuid(0x80000003, info1);
        ++info1;
        cpuid(0x80000004, info1);
        str[48] = 0;

        printf("# Model name: \t\t\t\t\t%s\n", str);
    }

    //CPU family and model

    cpuid(1, &info);
    cpu_model = ((info.array[0] >> 4) & 0xf) | ((info.array[0] >> 12) & 0xf0);
    printf("# Microarchitecture codename: \t\t\t%s\n", print_cpu_model(cpu_model));

    // MSR and Hyperthreading support

    msr = ((info.array[3] >> 5) & 0x1); 
    if (msr) printf("# MSR support: \t\t\t\t\tYES\n");
    else printf("# MSR support: \t\t\t\t\tNO\n");
    hyper = ((info.array[3] >> 28) & 0x1);
    if (hyper) printf("# Hyperthreading support: \t\t\tYES\n");
    else printf("# Hyperthreading support: \t\t\tNO\n");

    //PMU characteristics

    cpuid(0xa, &info);
    perf_version = (info.array[0] << 24) >> 24;
    max_gen_counter_num = (info.array[0] << 16) >> 24;
    gen_counter_width = (info.array[0] << 8) >> 24;
    if (perf_version > 1) {
        max_fixed_counter_num = (info.array[0] << 27) >> 27;
        fixed_counter_width = (info.array[0] << 19) >> 24;
    }
    printf("# Perf Version: \t\t\t\t%d\n# General Purpose Counters Number: \t\t%d\n# Gpcounters Width: \t\t\t\t%d\n# Fixed Counters Number: \t\t\t%d\n# Fcounters width: \t\t\t\t%d\n",
        perf_version, max_gen_counter_num, gen_counter_width, max_fixed_counter_num, fixed_counter_width);

    if (max_leaf > 7)
    {
        cpuid_leaf(0x07, 0, &info);
        if (info.reg.ebx & (1<<12))
        {
            cpuid_leaf(0x0f, 0, &info);
            if (info.reg.edx & (1<<1))
            {
                printf ("# Cache Monitoring Technology support: ");
                cpuid_leaf(0x0f, 1, &info);
                if (info.reg.edx & 1) 
                {
                    printf ("\t\tYES\n");
                    ExecSt.CMTAvailable = 1;
                }
                else printf ("\t\tNO\n");

                printf ("# Memory Bandwidth Monitoring (Total) support: ");
                if (info.reg.edx & (1<<1)) 
                {
                    printf("\tYES\n");
                    ExecSt.MBMTAvailable = 1;
                }
                else printf ("\tNO\n");

                printf ("# Memory Bandwidth Monitoring (Local) support: ");
                if (info.reg.edx & (1<<2)) 
                {
                    printf("\tYES\n");
                    ExecSt.MBMLAvailable = 1;
                }
                else printf ("\tNO\n");
            }
        }
    }

    cpuid(0x6, &info);
    if (info.reg.eax & 0x1) ExecSt.DTSAvailable = 1;
    if (info.reg.eax & (1<<6)) ExecSt.PLTAvailable = 1;
    if (info.reg.eax & 0x2) {
        printf("# Turbo mode: \t\t\t\t\tON\n");
        ExecSt.TurboModeON = 1;
    } else {
        printf("# Turbo mode: \t\t\t\t\tOFF\n");
        ExecSt.TurboModeON = 0;
    }

    printf("#############################################################################################\n\n");
}

void L3CMTGetCapabilities (L3Capabilities *L3cap)
{
    union CPUID_INFO info;

    cpuid_leaf(0x0f, 1, &info);

    L3cap->UpscalingFactor  = info.reg.ebx;
    L3cap->MaxRMID          = info.reg.ecx;
}

void InitRMID (aff_counters_t *cntrs)
{
    int i, cpus_nr;
    __u64 pqr_assoc_msr, qm_evtsel;
    __u64 rmid, maxrmid;
    __u64 event;
    prfcnt_t *handle;

    L3CMTGetCapabilities(&cntrs->L3Cap);
    maxrmid = cntrs->L3Cap.MaxRMID;
    cpus_nr = cntrs->cpus_nr;
    
    // cpus, given from command line, should correspond to those of socket 0. 
    // RMID are associated to cpus like following: [MaxRMID-cpu0], [(MaxRMID-1)-cpu1] 
    rmid = maxrmid;

    for(i = 0; i < cpus_nr; i++) {
        handle = &cntrs->prfcnt[i];

        // Association between core and RMID (Filling IA32_PQR_ASSOC register)

        prfcnt_rdmsr(handle->msr_fd, IA32_PQR_ASSOC, &pqr_assoc_msr);

        pqr_assoc_msr &= 0xfffffffffffffc00;

        if ((rmid > 0x4ff) || (rmid > maxrmid))
        {
            printf("rmid value exceeds the limits\n");
            exit(-1);
        }

        rmid &= 0x00000000000004ff;
        pqr_assoc_msr |= rmid;

        // printf("Associating CPU: %d with RMID: %lld and pqr_assoc_msr: %#18llx\n", handle->cpu, rmid, pqr_assoc_msr);

        prfcnt_wrmsr(handle->msr_fd, IA32_PQR_ASSOC, pqr_assoc_msr);

        // Select event in IA32_QM_EVTSEL (Filling RMID and EvtID)

        prfcnt_rdmsr(handle->msr_fd, IA32_QM_EVTSEL, &qm_evtsel);

        qm_evtsel &= 0xfffffc00ffffff00;

        event = (rmid<<32) | 0x0000000000000001;
        qm_evtsel |= event;

        prfcnt_wrmsr(handle->msr_fd, IA32_QM_EVTSEL, qm_evtsel);
        --rmid;
    }
}

void handleTurboMode (prfcnt_t *handle, char mode)
{
    __u64 data;

    prfcnt_rdmsr(handle->msr_fd, MSR_IA32_MISC_ENABLE, &data);
    
    switch (mode)
    {
        case 0: 
            data |= 1ULL<<38;
            break;
        case 1:
            data &= 0xffffffbfffffffff;
            break;
        default:
            printf("Mode not supported...Choose between ON(1) and OFF(0)\n");
    }
    
    prfcnt_wrmsr(handle->msr_fd, MSR_IA32_MISC_ENABLE, data);
}

void InitPower (aff_counters_t * cntrs)
{
    __u64       thermalSpecPower;
    __u64       minPower;
    __u64       maxPower;
    __u64       rapl_power_unit = 0;
    __u64       pkg_power_info = 0;
    double      powerUnit;
    prfcnt_t    *handle;

    handle = &cntrs->prfcnt[0];

    cntrs->energy_counters.pkg_energy_counter = 0;
    cntrs->energy_counters.pp0_energy_counter = 0;
    cntrs->energy_counters.dram_energy_counter = 0;

    if (ExecSt.TurboModeON) handleTurboMode(handle, 0);

    prfcnt_rdmsr(handle->msr_fd, MSR_RAPL_POWER_UNIT, &rapl_power_unit);

    powerUnit = cntrs->raplUnits.powerUnits = 1. / (double) (1<<(rapl_power_unit & 0xfULL));
    thermalSpecPower = (rapl_power_unit>>8) & 0x1fULL;
    cntrs->raplUnits.energyUnits = 1. / (double) (1<<((rapl_power_unit>>8) & 0x1fULL));
    cntrs->raplUnits.timeUnits = 1. / (double) (1<<((rapl_power_unit>>16) & 0xfULL));

    prfcnt_rdmsr(handle->msr_fd, MSR_PKG_POWER_INFO, &pkg_power_info);

    thermalSpecPower = (double) (pkg_power_info & 0x3fffULL);
    minPower = (double) ((pkg_power_info>>16) & 0x8fffULL);
    maxPower = (double) ((pkg_power_info>>32) & 0x8fffULL);

    printf("Package Power Info\nThermal Specification Power = %lf\nMinimum Power = %lf\nMaximum Power = %lf\n\n", thermalSpecPower * powerUnit, minPower * powerUnit, maxPower * powerUnit);
}

void startPower (aff_counters_t * cntrs)
{
    prfcnt_t        *handle;
    energyCounters  *current;

    current = &cntrs->energy_counters;
    handle = &cntrs->prfcnt[0];

    prfcnt_rdmsr(handle->msr_fd, MSR_PKG_ENERGY_STATUS, &current->prev_pkg_energy_counter);
    prfcnt_rdmsr(handle->msr_fd, MSR_PP0_ENERGY_STATUS, &current->prev_pp0_energy_counter);
    prfcnt_rdmsr(handle->msr_fd, MSR_DRAM_ENERGY_STATUS, &current->prev_dram_energy_counter);

    current->prev_pkg_energy_counter &= 0xffffffffULL;
    current->prev_pp0_energy_counter &= 0xffffffffULL;
    current->prev_dram_energy_counter &= 0xffffffffULL;
}

void stopPower (aff_counters_t * cntrs)
{
    __u64           pkgEnergy;
    __u64           pp0Energy;
    __u64           dramEnergy;
    prfcnt_t        *handle;
    energyCounters  *last;

    last = &cntrs->energy_counters;
    handle = &cntrs->prfcnt[0];

    prfcnt_rdmsr(handle->msr_fd, MSR_PKG_ENERGY_STATUS, &pkgEnergy);
    prfcnt_rdmsr(handle->msr_fd, MSR_PP0_ENERGY_STATUS, &pp0Energy);
    prfcnt_rdmsr(handle->msr_fd, MSR_DRAM_ENERGY_STATUS, &dramEnergy);

    pkgEnergy &= 0xffffffffULL;
    pp0Energy &= 0xffffffffULL;
    dramEnergy &= 0xffffffffULL;

    if (pkgEnergy >= last->prev_pkg_energy_counter) last->pkg_energy_counter = pkgEnergy - last->prev_pkg_energy_counter;
    else last->pkg_energy_counter = ((1ULL<<32) - last->prev_pkg_energy_counter) + pkgEnergy;

    if (pp0Energy >= last->prev_pp0_energy_counter) last->pp0_energy_counter = pp0Energy - last->prev_pp0_energy_counter;
    else last->pp0_energy_counter = ((1ULL<<32) - last->prev_pp0_energy_counter) + pp0Energy;

    if (dramEnergy >= last->prev_dram_energy_counter) last->dram_energy_counter = dramEnergy - last->prev_dram_energy_counter;
    else last->dram_energy_counter = ((1ULL<<32) - last->prev_dram_energy_counter) + dramEnergy;
}

double getEnergyCounters (aff_counters_t * cntrs, char type)
{
    energyCounters  *counters;
    powerInfo       *info;
    double          data;

    info = &cntrs->raplUnits;
    counters = &cntrs->energy_counters;

    switch (type)
    {
        case PKG_ENERGY_COUNTER:
            data = ((double) counters->pkg_energy_counter) * info->energyUnits;
            break;
        case PP0_ENERGY_COUNTER:
            data = ((double) counters->pp0_energy_counter) * info->energyUnits;
            break;
        case DRAM_ENERGY_COUNTER:
            data = ((double) counters->dram_energy_counter) * 0.0000153;
            break;
        default:
            data = 0;
    }

    return data;
}

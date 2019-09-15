#ifndef __PRFCNT_NEHALEM_TYPES_H__
#define __PRFCNT_NEHALEM_TYPES_H__

typedef enum {
    PMC0 = 0,
    PMC1,  PMC2,  PMC3,  PMC4,  PMC5,  PMC6,
    PMC7,  PMC8,  PMC9,  PMC10, PMC11, PMC12,
    PMC13, PMC14, PMC15, PMC16, PMC17, PMC18,
    PMC19, PMC20, PMC21, PMC22, PMC23, PMC24,
    PMC25, PMC26, PMC27, PMC28, PMC29, PMC30,
    PMC31, PMC32, PMC33, PMC34, PMC35, PMC36,
    PMC37, PMC38, PMC39, PMC40, PMC41, PMC42,
    PMC43, PMC44, PMC45, PMC46, PMC47,
    NUM_PMC
} CounterIndex;

typedef enum {
    PMC = 0,
    FIXED,
    UNCORE,
    MBOX0,
    MBOX1,
    MBOX2,
    MBOX3,
    MBOXFIX,
    BBOX0,
    BBOX1,
    RBOX0,
    RBOX1,
    WBOX,
    SBOX0,
    SBOX1,
    SBOX2,
    CBOX,
    PBOX,
    POWER,
    NUM_UNITS
} CounterType;

typedef struct {
        char*           key;
        CounterIndex    index;
} CounterMap;

typedef struct {
        CounterType     type;
        int             init;
        int             id;
        __u64           configRegister;
        __u64           counterRegister;
        __u64           counterRegister2;
        __u64           counterData;
} Counter;

struct off_rsp
{
    union
    {
        struct 
        {
            __u64   dmnd_data_rd : 1;
            __u64   dmnd_rfo : 1;
            __u64   dmnd_ifetch : 1;
            __u64   corewb : 1;
            __u64   pf_data_rd :1;
            __u64   pf_rfo : 1;
            __u64   pf_ifetch : 1;
            __u64   pf_l3_data_rd : 1;
            __u64   pf_l3_rfo : 1;
            __u64   pf_l3_code_rd : 1;
            __u64   split_lock_uc_lock : 1;
            __u64   strm_st : 1;
            __u64   reserved : 3;
            __u64   other : 1;
            __u64   any : 1;
            __u64   no_supp : 1;
            __u64   l3_hitm : 1;
            __u64   l3_hite : 1;
            __u64   l3_hits : 1;
            __u64   l3_hitf : 1;
            __u64   local : 1;
            __u64   reserved2 : 4;
            __u64   l3_miss_remote_hop0 : 1;
            __u64   l3_miss_remote_hop1 : 1;
            __u64   l3_miss_remote_hop2p : 1;
            __u64   reserved3 : 1;
            __u64   snp_none : 1;
            __u64   snp_not_needed : 1;
            __u64   snp_miss : 1;
            __u64   snp_no_fwd : 1;
            __u64   snp_fwd : 1;
            __u64   hitm : 1;
            __u64   non_dram : 1;
            __u64   reserved4 : 26;
        }fields;
        __u64 value;
    };
};

struct prfcnt_core_event {
	__u8            evnt;
	__u8            umask;
	__u8            cfgBits;
	__u8            cmask;
	struct off_rsp	off_mask;
	__u64		vsel_mask;
	unsigned int    type;
	char            *desc;
};
typedef struct prfcnt_core_event prfcnt_event_t;

typedef struct 
{
    __u64       configRegister;
    __u64       counterRegister;
}event_info;

struct prfcnt_core_handle {
        int             L3COMetricAvailable:1;
        int             DTSAvailable:1;
        int             PLTAvailable:1;
        event_info      L3OccupancyInfo;
        event_info      DTSInfo;
        event_info      PLTInfo;
        int             msr_fd;
        unsigned int    cpu;
        unsigned long   flags;
        prfcnt_event_t  **events;
        unsigned int    events_nr;
        Counter         counters[NUM_COUNTERS];
};
typedef struct prfcnt_core_handle prfcnt_t;

#define MSR_IA32_DISABLE_PREF           0x1A4
#define MSR_IA32_THERM_STATUS           0x19C
#define MSR_IA32_PACKAGE_THERM_STATUS   0x1B1
#define MSR_TEMPERATURE_TARGET          0x1A2
#define MSR_IA32_PERF_CTL               0x199

/* MSR Registers  */
#define IA32_PQR_ASSOC          0xC8F
#define IA32_QM_EVTSEL          0xC8D
#define IA32_QM_CTR             0xC8E

/* 3 80 bit fixed counters */
#define MSR_PERF_FIXED_CTR_CTRL   0x38D
#define MSR_PERF_FIXED_CTR0       0x309  /* Instr_Retired.Any */
#define MSR_PERF_FIXED_CTR1       0x30A  /* CPU_CLK_UNHALTED.CORE */
#define MSR_PERF_FIXED_CTR2       0x30B  /* CPU_CLK_UNHALTED.REF */
/* 4 40/48 bit configurable counters */
/* Perfmon V1 */
#define MSR_PERFEVTSEL0           0x186
#define MSR_PERFEVTSEL1           0x187
#define MSR_PERFEVTSEL2           0x188
#define MSR_PERFEVTSEL3           0x189
#define MSR_PERFEVTSEL4           0x18A
#define MSR_PERFEVTSEL5           0x18B
#define MSR_PERFEVTSEL6           0x18C
#define MSR_PERFEVTSEL7           0x18D
#define MSR_PMC0                  0x0C1
#define MSR_PMC1                  0x0C2
#define MSR_PMC2                  0x0C3
#define MSR_PMC3                  0x0C4
#define MSR_PMC4                  0x0C5
#define MSR_PMC5                  0x0C6
#define MSR_PMC6                  0x0C7
#define MSR_PMC7                  0x0C8
/* Perfmon V2 */
#define MSR_PERF_GLOBAL_CTRL      0x38F
#define MSR_PERF_GLOBAL_STATUS    0x38E
#define MSR_PERF_GLOBAL_OVF_CTRL  0x390
#define MSR_PEBS_ENABLE           0x3F1
/* Perfmon V3 */
#define MSR_OFFCORE_RSP0              0x1A6
#define MSR_OFFCORE_RSP1              0x1A7
#define MSR_UNCORE_PERF_GLOBAL_CTRL       0x391
#define MSR_UNCORE_PERF_GLOBAL_STATUS     0x392
#define MSR_UNCORE_PERF_GLOBAL_OVF_CTRL   0x393
#define MSR_UNCORE_FIXED_CTR0             0x394  /* Uncore clock cycles */
#define MSR_UNCORE_FIXED_CTR_CTRL         0x395 /*FIXME: Is this correct? */
#define MSR_UNCORE_ADDR_OPCODE_MATCH      0x396 
#define MSR_UNCORE_PERFEVTSEL0         0x3C0
#define MSR_UNCORE_PERFEVTSEL1         0x3C1
#define MSR_UNCORE_PERFEVTSEL2         0x3C2
#define MSR_UNCORE_PERFEVTSEL3         0x3C3
#define MSR_UNCORE_PERFEVTSEL4         0x3C4
#define MSR_UNCORE_PERFEVTSEL5         0x3C5
#define MSR_UNCORE_PERFEVTSEL6         0x3C6
#define MSR_UNCORE_PERFEVTSEL7         0x3C7
#define MSR_UNCORE_PMC0                0x3B0
#define MSR_UNCORE_PMC1                0x3B1
#define MSR_UNCORE_PMC2                0x3B2
#define MSR_UNCORE_PMC3                0x3B3
#define MSR_UNCORE_PMC4                0x3B4
#define MSR_UNCORE_PMC5                0x3B5
#define MSR_UNCORE_PMC6                0x3B6
#define MSR_UNCORE_PMC7                0x3B7

/* Power interfaces, RAPL */
#define MSR_RAPL_POWER_UNIT             0x606
#define MSR_PKG_RAPL_POWER_LIMIT        0x610
#define MSR_PKG_ENERGY_STATUS           0x611
#define MSR_PKG_PERF_STATUS             0x613
#define MSR_PKG_POWER_INFO              0x614
#define MSR_PP0_RAPL_POWER_LIMIT        0x638
#define MSR_PP0_ENERGY_STATUS           0x639
#define MSR_PP0_ENERGY_POLICY           0x63A
#define MSR_PP0_PERF_STATUS             0x63B
#define MSR_PP1_RAPL_POWER_LIMIT        0x640
#define MSR_PP1_ENERGY_STATUS           0x641
#define MSR_PP1_ENERGY_POLICY           0x642
#define MSR_DRAM_RAPL_POWER_LIMIT       0x618
#define MSR_DRAM_ENERGY_STATUS          0x619
#define MSR_DRAM_PERF_STATUS            0x61B
#define MSR_DRAM_POWER_INFO             0x61C

/* Turbo Boost Interface */
#define MSR_IA32_MISC_ENABLE            0x1A0
#define MSR_PLATFORM_INFO               0x0CE
#define MSR_TURBO_POWER_CURRENT_LIMIT   0x1AC
#define MSR_TURBO_RATIO_LIMIT           0x1AD

#define SOCKET0_BUS "7f"
#define SOCKET1_BUS "ff"

#define MC0_CHANNEL0_DEV_ADDR 14
#define MC0_CHANNEL1_DEV_ADDR 14
#define MC0_CHANNEL2_DEV_ADDR 15
#define MC0_CHANNEL3_DEV_ADDR 15
#define MC1_CHANNEL0_DEV_ADDR 17
#define MC1_CHANNEL1_DEV_ADDR 17
#define MC1_CHANNEL2_DEV_ADDR 18
#define MC1_CHANNEL3_DEV_ADDR 18

#define MC0_CHANNEL0_FUNC_ADDR 0
#define MC0_CHANNEL1_FUNC_ADDR 1

#define MC_CH_PCI_PMON_CTL_EVENT(x) (x << 0)
#define MC_CH_PCI_PMON_CTL_UMASK(x) (x << 8)

#define XPF_MC_CH_PCI_PMON_BOX_CTL_ADDR (0x0F4)
#define KNX_MC_CH_PCI_PMON_FIXED_CTL_ADDR (0xB44)

#define XPF_MC_CH_PCI_PMON_FIXED_CTL_ADDR (0x0F0)
#define XPF_MC_CH_PCI_PMON_CTL3_ADDR (0x0E4)
#define XPF_MC_CH_PCI_PMON_CTL2_ADDR (0x0E0)
#define XPF_MC_CH_PCI_PMON_CTL1_ADDR (0x0DC)
#define XPF_MC_CH_PCI_PMON_CTL0_ADDR (0x0D8)

#define XPF_MC_CH_PCI_PMON_FIXED_CTR_ADDR (0x0D0)
#define XPF_MC_CH_PCI_PMON_CTR3_ADDR (0x0B8)
#define XPF_MC_CH_PCI_PMON_CTR2_ADDR (0x0B0)
#define XPF_MC_CH_PCI_PMON_CTR1_ADDR (0x0A8)
#define XPF_MC_CH_PCI_PMON_CTR0_ADDR (0x0A0)

#define MC_CH_PCI_PMON_BOX_CTL_RST_CONTROL  (1 << 0)
#define MC_CH_PCI_PMON_BOX_CTL_RST_COUNTERS     (1 << 1)
#define MC_CH_PCI_PMON_BOX_CTL_FRZ  (1 << 8)
#define MC_CH_PCI_PMON_BOX_CTL_FRZ_EN   (1 << 16)

#define MC_CH_PCI_PMON_FIXED_CTL_RST (1 << 19)
#define MC_CH_PCI_PMON_FIXED_CTL_EN (1 << 22)

#define MC_CH_PCI_PMON_CTL_EN (1 << 22)


#endif

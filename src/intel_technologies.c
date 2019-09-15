#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEHALEM_EP          26
#define NEHALEM             30
#define ATOM                28
#define ATOM_2              53
#define ATOM_CENTERTON      54
#define ATOM_BAYTRAIL       55
#define ATOM_AVOTON         77
#define ATOM_CHERRYTRAIL    76
#define ATOM_APOLLO_LAKE    92
#define CLARKDALE           37
#define WESTMERE_EP         44
#define NEHALEM_EX          46
#define WESTMERE_EX         47
#define SANDY_BRIDGE        42
#define JAKETOWN            45
#define IVY_BRIDGE          58
#define HASWELL             60
#define HASWELL_ULT         69
#define HASWELL_2           70
#define IVYTOWN             62
#define HASWELLX            63
#define BROADWELL           61
#define BROADWELL_XEON_E3   71
#define BDX_DE              86
#define SKL_UY              78
#define BDX                 79
#define KNL                 87
#define SKL                 94
#define ATOM_DENVERTON      95
#define KBL                 158
#define KBL_1               142
#define SKX                 85

union CPUID_INFO
{
    int array[4];
    struct { unsigned int eax, ebx, ecx, edx; } reg;
};

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
        case KBL:
            return "Kabylake";
        case SKX:
            return "Skylake-SP";
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

int main()
{
    union {
        char cbuf[16];
        int ibuf[16/sizeof(int)];
    }buf;

    union CPUID_INFO info, *info1;
    unsigned int cpu_model, msr, hyper, perf_version, max_gen_counter_num, gen_counter_width; //cpu_family;
    unsigned int fixed_counter_width = 0, max_fixed_counter_num = 0;
    int max_leaf;
    char str[49], cmt=0, l3cat=0, l2cat=0, mbmt=0, mbml=0, mba=0, cdp=0;

    printf("\n#############################################################################################\n");
    printf("#                                    CPU CHARACTERISTICS                                    #\n");
    printf("#############################################################################################\n");

    cpuid(0, &info);

    // printf("EAX = 0, Highest calling parameter = %#x\n", info[0]);

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
    //printf("EAX = 800000000, Highest calling parameter = %#x\n", info[0]);

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

    // cpu_family = ((info.array[0] & 0xf0000000) >> 16) | ((info.array[0] >> 8) & 0xf);
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

    if (max_leaf > 7) {
        cpuid_leaf(0x07, 0, &info);
        if (info.reg.ebx & (1<<12)) {
            cpuid_leaf(0x0f, 0, &info);
            if (info.reg.edx & (1<<1)) {
                cpuid_leaf(0x0f, 1, &info);
                if (info.reg.edx & 1) cmt = 1;
                if (info.reg.edx & (1<<1)) mbmt = 1;
                if (info.reg.edx & (1<<2)) mbml = 1;
            }
        }

        cpuid_leaf(0x07, 0, &info);
        if (info.reg.ebx & (1<<15)) {
            cpuid_leaf(0x10, 0, &info);
            if (info.reg.ebx & (1<<1)) l3cat = 1;
            if (info.reg.ebx & (1<<2)) l2cat = 1;
            if (info.reg.ebx & (1<<3)) mba = 1;

            cpuid_leaf(0x10, 1, &info);
            if (info.reg.ecx & (1<<2)) cdp = 1;
        }
    }

    printf ("# Cache Monitoring Technology: ");
    if (cmt) printf("\t\t\tYES\n");
    else printf("\t\t\tNO\n");
    printf ("# Memory Bandwidth Monitoring (Total): ");
    if (mbmt) printf("\t\tYES\n");
    else printf("\t\tNO\n");
    printf ("# Memory Bandwidth Monitoring (Local): ");
    if (mbml) printf("\t\tYES\n");
    else printf("\t\tNO\n");
    printf ("# L3 Cache Allocation Technology: ");
    if (l3cat) printf("\t\tYES\n");
    else printf("\t\tNO\n");
    printf ("# L2 Cache Allocation Technology:");
    if (l2cat) printf("\t\tYES\n");
    else printf("\t\tNO\n");
    printf ("# Memory Bandwidth Allocation Technology: ");
    if (mba) printf("\tYES\n");
    else printf("\tNO\n");
    printf ("# Code and Data Prioritization Technology: ");
    if (cdp) printf("\tYES\n");
    else printf("\tNO\n");

    cpuid(0x6, &info);

    // if (info.reg.eax & 0x1) ExecSt.DTSAvailable = 1;
    // if (info.reg.eax & (1<<6)) ExecSt.PLTAvailable = 1;

    if (info.reg.eax & 0x2) printf("# Turbo mode: \t\t\t\t\tON\n");
    else printf("# Turbo mode: \t\t\t\t\tOFF\n");

    printf("#############################################################################################\n\n");
}

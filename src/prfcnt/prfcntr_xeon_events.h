#ifndef __PRFCNT_NEHALEM_EVENTS_H__     
#define __PRFCNT_NEHALEM_EVENTS_H__

#define EVENT_NONE              -1
#define PROG_EVENT				0
#define TEMP_EVENT				1
#define RMID_EVENT				2
#define PKG_ENERGY_COUNTER		3
#define PP0_ENERGY_COUNTER		4
#define DRAM_ENERGY_COUNTER		5

#include "prfcntr_xeon_types.h"  

/* programmable events */
typedef enum {
	EVENT_UNHLT_CORE_CYCLES = 0,
	EVENT_INSTR_RETIRED,
	EVENT_LLC_REFERENCE,
	EVENT_LLC_MISSES,
	EVENT_L1D_REPLACEMENT,
	EVENT_L2_LINES_IN,
	EVENT_OFFCORE_REQUESTS_RD_BANDWIDTH,
	/* end of core events */
	FIXED_EVENTS_OFFSET
} ProgEvents;

/* fixed events */
typedef enum {
	EVENT_FIXED_0 = FIXED_EVENTS_OFFSET,
	EVENT_FIXED_1,
	EVENT_FIXED_2
} FixedEvents;

static int __evnts_selected[] = {
	EVENT_FIXED_0,
	EVENT_FIXED_1,
	EVENT_FIXED_2,
	EVENT_LLC_MISSES,
	EVENT_L1D_REPLACEMENT,			// L2 -> L1 bandwidth [MBytes/s] = 1.0E-06*(L1D_REPLACEMENT+L1D_M_EVICT)*64/time
	EVENT_L2_LINES_IN,				// L3 -> L2 bandwidth
	EVENT_OFFCORE_REQUESTS_RD_BANDWIDTH,
};

static prfcnt_event_t __evnts[] = {
	[EVENT_LLC_MISSES] = 
	{ .evnt = 0x2e, .umask = 0x41, .type = PMC, .desc = "LLC_MISSES" },
	[EVENT_L1D_REPLACEMENT] = 
	{.evnt = 0x51, .umask = 0x01, .type = PMC, .desc = "Counts the number of lines brought into the L1 data cache." },
	[EVENT_UNHLT_CORE_CYCLES] = 
	{ .evnt = 0x3c, .umask = 0x00, .type = PMC,    .desc = "UNHLT_CORE_CYCLES" },
	[EVENT_INSTR_RETIRED] = 
	{ .evnt = 0xc0, .umask = 0x00, .type = PMC,    .desc = "INSTR_RETIRED" },
	[EVENT_LLC_REFERENCE] = 
	{ .evnt = 0x2e, .umask = 0x4f, .type = PMC,    .desc = "LLC_REFERENCE"  },
	[EVENT_OFFCORE_REQUESTS_RD_BANDWIDTH] = 
	{.evnt = 0xB7, .umask = 0x01, .type = PMC, .off_mask.value = 0x800101b3,   .desc = "" },
	[EVENT_L2_LINES_IN] = 
	{ .evnt = 0xF1, .umask = 0x07, .type = PMC,    .desc = "L2_LINES_IN_ALL" },
};

#endif

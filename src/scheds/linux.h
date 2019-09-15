#ifndef __B_GANG_H__
#define __B_GANG_H__

#include "../aff-executor.h"
#include "../stats.h"

struct linux_sched {
        list_t  progs;
        int                     progs_nr;
        int                     progs_init;
        int                     progs_left;
        int                     stop_execution;
        aff_counters_t		*cntrs;
		FILE *totalsF;
		FILE *targetF;
		FILE *energyF;
		int sched_time;
		double *total_rtime;
		double *finish;
		double *quanta;
		double *average_rtime;
		double *times_target;
		double *cur_time;
		int *prog_times_finished;
		char **total_progs;
		int total_tics;
//		pid_t	pid;
};
typedef struct linux_sched linux_sched_t;

struct linux_data {
	linux_sched_t *sched;
	int init_id;
	char *prg;
	clockid_t clockid;
	struct timespec ts;
	double msecs;
	char killed;
};
typedef struct linux_data linux_prog_data_t;

void * linux_init(void); 
void linux_rebalance(void *arg); 
void linux_qexpired(void *arg, struct timeval *now); 

#endif

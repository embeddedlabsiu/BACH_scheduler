#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include "../aff-executor.h"
#include "c_qsort.h"
#include "../list.h"
#include "../stats.h"
#include "common.h"

// #define TIME_WINDOW
#define MAX_GANGS		5
#define RUN_TICS        280
#define BW_LIMIT		547208745

static int monitoring_phase(sched_t *sched) {
	if (sched->monitor_wnd > sched->progs_nr)
		return 0;
	else 
		return 1;
}

void perf_fair_elem_move2tail(list_t *elem, list_t *to_list) {
	cds_list_del(elem);
	CDS_INIT_LIST_HEAD(elem);
	cds_list_add_tail(elem, to_list);
}

void perf_fair_move_progs_to_all(sched_t *sched){
	aff_prog_t	*p, *p_tmp;
	list_t		*apl = &sched->progs_all;
	list_t		*spl = &sched->progs_sch;

	if(!cds_list_empty(spl)){
		cds_list_for_each_entry_safe(p, p_tmp, spl, pl_node) {
			perf_fair_elem_move2tail(&p->pl_node, apl);
		}
	}
}

int perf_fair_get_value(list_t *l){
	aff_prog_t		*prog;
	prog_data_t		*pd;

	prog 	= cds_list_entry(l, aff_prog_t, pl_node);
	pd		= prog->sched_data;
	return 	pd->ratio;
}

void perf_fair_update_progress(prog_data_t *pd, __u64 unh_cycles) {
	if (unh_cycles)
	{
		pd->ideal_instr		+=	(pd->ipc_alone * unh_cycles) / 1000;
		pd->ratio			=	(pd->instr_retd_sum * 1000) / pd->ideal_instr;	
	}
}

void perf_fair_collect(sched_t *sched, prog_data_t *pd) {
	offcntrs *offcore = &sched->offcoreCounters;

	pd->ipc_alone = (pd->instr_retd * 1000) / pd->unh_cycles;
	pd->sum_mem_reads = (pd->category == BW) ? offcore->UncMCReads : pd->mem_reads;
	pd->llc_alone = pd->llc_occupancy;
	// fprintf(sched->finishedF, "prog: %20s, IPC ALONE: %7d, Target Instr: %14lld\n", pd->prg, pd->ipc_alone, sched->target_instr[pd->init_id]);
	printf("prog: %s, IPC: %d, Mem BW: %llu, LLC OCC: %d\n", pd->prg, pd->ipc_alone, pd->sum_mem_reads, pd->llc_alone);
}

/****************** Termination Management *****************/

void perf_fair_kill_app(sched_t *sched, aff_prog_t *prog)
{
	prog_data_t  	*pd;
	pid_t			pid;
	struct timeval tms;

	gettimeofday(&tms, NULL);

	printf("Function: %s\n", __func__);
	pd = prog->sched_data;
	cds_list_del(&prog->pl_node);
	CDS_INIT_LIST_HEAD(&prog->pl_node);
	pd->killed = 1;
	sched_update_totals(sched, prog);
	fprintf(stderr, "Sending SIGKILL to prog: %s with pid: %d\n", prog->cmd, prog->pid);
	pid = prog->pid;
	sched_clear_prog(prog);
	// sched_del_prog(prog);
	kill(pid, SIGKILL);
	sched->progs_left--;
	sched->progs_fnsh++;
	sched->progs_nr--;
	if (sched->progs_left <= 0)	{
		sched_print_totals(sched);
		sched_finalization(sched);
		printf("TERMINATING!!!!!!!\n");
	}
}

void perf_fair_kill_all(sched_t *sched){
	aff_prog_t      *prog, *tmp_prog;
	prog_data_t  	*pd;
	list_t			*pall = &sched->progs_all;
	int 			i;
	pid_t			pid;

	printf("Function: %s\n", __func__);
	perf_fair_move_progs_to_all(sched);
	if (!cds_list_empty(pall)) {
		cds_list_for_each_entry_safe(prog, tmp_prog, pall, pl_node)	{
			pd = prog->sched_data;
			cds_list_del(&prog->pl_node);
			CDS_INIT_LIST_HEAD(&prog->pl_node);
			i = pd->init_id;
			sched->last_instr[i] = get_total_instr(pd->stats);
			sched->total_instr[i] += sched->last_instr[i];
			sched->total_cycles[i] += get_total_cycles(pd->stats);
			sched_update_clock(pd);
			sched->total_rtime[i] += pd->tm_running;
			sched->total_wtime[i] += pd->tm_waiting;
			fprintf(stderr, "Sending SIGKILL to prog: %s with pid: %d\n", prog->cmd, prog->pid);
			pid = prog->pid;
			sched->progs_fnsh++;
			sched->progs_nr--;
			sched_clear_prog(prog);
			kill(pid, SIGKILL);
		}
	}	
}

/********************* Scheduler's Basic Functions ***********************/

void *perf_fair_init(void){
	sched_t 		*ret;
	int i;
	struct timeval tms;
	char target_path[600], *append = "target";

	print_cpu_characteristics();

	if( !(ret = calloc(1, sizeof(*ret))) ) perrdie("calloc", "could not initialize scheduler");

	CDS_INIT_LIST_HEAD(&ret->progs_all);
	CDS_INIT_LIST_HEAD(&ret->progs_sch);

	ret->cntrs		= ss_aff_counters_init(ExecSt.cpus_nr, ExecSt.cpus);
	if (ExecSt.CMTAvailable) InitRMID(ret->cntrs);
	InitPower(ret->cntrs);
	startPower(ret->cntrs);
	startMCCounters(ret->cntrs);
	ss_aff_stop_counters(ret->cntrs);
	ss_aff_zero_cntrs(ret->cntrs);
	sched_open_results_files(ret);
	sched_init_counters_output(ret);
	
	ret->progs_init 		= ret->progs_left = ExecSt.waiting_nr;
	ret->progs_nr			= 0;
	ret->monitor_wnd		= 0;
	ret->progs_fnsh 		= 0;
	ret->stop_execution 	= 0;
	ret->do_respawn			= 1;
	ret->total_tics			= -1;
	ret->unh_cycles 		= 0;
	ret->offcoreCounters.pkgEnergyCounter 	= 0;
	ret->offcoreCounters.dramEnergyCounter 	= 0;
	ret->offcoreCounters.UncMCReads 		= 0;
	ret->offcoreCounters.UncMCWrites 		= 0;
	ret->average_bandwidth					= 0;

	if ((ret->instr_progs = malloc(ret->progs_init * sizeof(__u64))) == NULL) perrdie ("malloc", "");
	if ((ret->last_instr = malloc(ret->progs_init * sizeof(__u64))) == NULL) perrdie ("malloc", "");
	if ((ret->total_progs = malloc(ret->progs_init * sizeof(char *))) == NULL) perrdie ("malloc", "");
	if ((ret->prog_times_finished = malloc(ret->progs_init * sizeof(int))) == NULL) perrdie ("malloc","");
	if ((ret->total_rtime = malloc(ret->progs_init * sizeof(unsigned int))) == NULL) perrdie ("malloc","");
	if ((ret->total_wtime = malloc(ret->progs_init * sizeof(unsigned int))) == NULL) perrdie ("malloc","");
	if ((ret->total_instr = malloc(ret->progs_init * sizeof(__u64))) == NULL) perrdie ("malloc", "");
	if ((ret->total_cycles = malloc(ret->progs_init * sizeof(__u64))) == NULL) perrdie ("malloc", "");
	if ((ret->quanta = malloc(ret->progs_init * sizeof(int))) == NULL) perrdie ("malloc","");
	if ((ret->finish_time = malloc(ret->progs_init * sizeof(unsigned int))) == NULL) perrdie ("malloc","");
	if ((ret->target_instr = calloc(ret->progs_init, sizeof(__u64))) == NULL) perrdie ("malloc", "");

	for (i=0; i<ret->progs_init; i++){
		ret->total_progs[i] = NULL;
		ret->prog_times_finished[i] = 0;
		ret->last_instr[i] 	= 0;
		ret->instr_progs[i] = 0;
		ret->total_rtime[i] = 0;
		ret->total_wtime[i] = 0;
		ret->target_instr[i]	= 0;
		ret->quanta[i] 			= 0;
		ret->total_instr[i]		= 0;
		ret->total_cycles[i]	= 0;
		ret->finish_time[i] 	= 0;
  	}

	snprintf(target_path, sizeof(target_path), "%s_%s", instr_file, append);
	if ((ret->targetF = fopen(target_path, "r")) == NULL) {
		perror("open:");
		exit(-1);
	} else read_instr_from_file(ret);

	qt_event.tv.tv_sec      = qt.tv_sec;
	qt_event.tv.tv_usec     = qt.tv_usec;
	heap_push(ExecSt.events_heap, (unsigned long)&qt_event);

	gettimeofday(&tms, NULL);
	ret->sched_start = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);

	return ret; 
}

void perf_fair_thaw_sched(sched_t *sched){
	aff_prog_t     	*prog;
	prog_data_t  	*pd;
	list_t			*pl = &sched->progs_sch;

	cds_list_for_each_entry(prog, pl, pl_node){
		pd = prog->sched_data;
		ss_aff_zero_stats(pd->stats);
		ss_aff_stats_start(pd->stats, prog->cpuset);	// start program's counter stats
		sched_update_clock(pd);							// update clocks
		pd->run_status	= RS_RUNNING;					// program thawed
		frzr_setstate(prog->idpath, FRZR_THAWED);		// thaw program
	}	
	ss_aff_zero_cntrs(sched->cntrs);					// zero counters
	ss_aff_counters_start(sched->cntrs);				// start counters
	startMCCounters(sched->cntrs);
	startPower(sched->cntrs);
}

void perf_fair_freeze_sched(sched_t *sched){
	aff_prog_t      *prog, *tmp;
	prog_data_t  	*pd;
	list_t          *pl  = &sched->progs_sch;
	list_t          *apl = &sched->progs_all;
	__u64			unh_cycles = 0;

	ss_aff_stop_counters(sched->cntrs);				// stop counters
	ss_aff_read_counters(sched->cntrs);				// read counters
	stopMCCounters(sched->cntrs);
	stopPower(sched->cntrs);

	get_offcore_metrics(sched);
	if (sched->total_tics == 0) sched_initial_metrics(sched);

	cds_list_for_each_entry_safe(prog, tmp, pl, pl_node) {
		frzr_setstate(prog->idpath, FRZR_FROZEN);	// stop program
		pd = prog->sched_data;				 
		sched_update_clock(pd);						// update clocks
		pd->run_status	= RS_STOPED;				// program stoped
	}

	cds_list_for_each_entry_safe(prog, tmp, pl, pl_node) {
		pd = prog->sched_data;				 
		ss_aff_stats_pause(pd->stats, prog->cpuset);
		ss_aff_stats_read(pd->stats, prog->cpuset);

		sched_update_prfm(prog);					// update performance counters
		++pd->times_cpu;
		pd->avg_mem_reads = (pd->instr_retd * 1000) / pd->unh_cycles; 		// co-running IPC
		sched_print_counters(prog, sched);
		if (!unh_cycles) unh_cycles = pd->unh_cycles;
		if (monitoring_phase(sched)) perf_fair_collect(sched, pd);
		else {
			pd->instr_retd_sum += pd->instr_retd;
			sched->target_instr[pd->init_id] -= (long long int) pd->instr_retd;
			if (sched->target_instr[pd->init_id] <= 0) perf_fair_kill_app(sched, prog);
		}
	}

	perf_fair_move_progs_to_all(sched);					// ensure that all progs are in all list
	fprintf(sched->scheduleF,"Updating progress ratio...\n");
	cds_list_for_each_entry(prog, apl, pl_node)	{
		pd = prog->sched_data;
		sched_update_waiting(pd);
		if (!monitoring_phase(sched)) perf_fair_update_progress(pd, unh_cycles);
		fprintf(sched->scheduleF,"\tPROG: %20s  ON_INSTR: %14llu  IDEAL_INSTR: %14llu  ON_INSTR/IDEAL:%12llu  WAITING TIME: %6d CLASS: %3d BW_ALONE: %10.8lf LLC_ALONE: %10.8lf IPC_CO/IPC_ALONE: %10.8lf TARGET INSTR:$ %14lld\n", pd->prg, pd->instr_retd_sum, pd->ideal_instr, pd->ratio, pd->waiting_time, pd->category, (double) (pd->sum_mem_reads * 64) / 1000000.0, (double) pd->llc_alone / 1000.0, (double) pd->avg_mem_reads / (double) pd->ipc_alone, sched->target_instr[pd->init_id]);
	}
	if (sched->monitor_wnd == sched->progs_nr) sched->monitor_wnd++;
}

static aff_prog_t * good_neighbors(sched_t *sched, long long int bw)
{
	aff_prog_t		*prog, *tmp, *min;
	prog_data_t  	*pd, *min_pd;
	list_t			*all = &sched->progs_all;
	list_t			*sch = &sched->progs_sch;
	long long int min_fitness, fitness;

	min = cds_list_entry(all->next, aff_prog_t, pl_node);
	min_pd = min->sched_data;
	min_fitness = (bw >= min_pd->sum_mem_reads) ? (bw - min_pd->sum_mem_reads) : (min_pd->sum_mem_reads - bw);

	cds_list_for_each_entry_safe(prog, tmp, all, pl_node) {
		pd = prog->sched_data;
		fitness = (bw >= pd->sum_mem_reads) ? (bw - pd->sum_mem_reads) : (pd->sum_mem_reads - bw);
		if (fitness < min_fitness) {
			min = prog;
			min_pd = pd;
			min_fitness = fitness;
		}
	}

	perf_fair_elem_move2tail(&min->pl_node, sch);

	return min;
}

static void fill_rest_cores(sched_t *sched, int cores_allocated)
{
	aff_prog_t		*prog, *tmp;
	prog_data_t  	*pd;
	list_t			*all = &sched->progs_all;
	list_t			*sch = &sched->progs_sch;
	int cores=cores_allocated;

	cds_list_for_each_entry_safe(prog, tmp, all, pl_node) {
		if (cores == MAX_CORES) break;
		pd = prog->sched_data;
		fprintf(sched->scheduleF,"\tPROG: %20s  ON_INSTR: %14llu  IDEAL_INSTR: %14llu  ON_INSTR/IDEAL:%12llu  WAITING TIME: %6d BW_ALONE: %10.8lf\n", pd->prg, pd->instr_retd_sum, pd->ideal_instr, pd->ratio, pd->waiting_time, (double) (pd->sum_mem_reads * 64) / 1000000.0);
		perf_fair_elem_move2tail(&prog->pl_node, sch);
		schedule_setcpurange(prog, cores, cores + prog->cores_needed);
		schedule_reattach(prog);
		pd->cpu = get_cpus(prog);
		cores += prog->cores_needed;
	}
}

static void monitor(sched_t *sched) {
	aff_prog_t		*prog;
	prog_data_t		*pd;
	list_t			*all = &sched->progs_all;
	int core;

	core = sched->monitor_wnd % MAX_CORES;
	prog = cds_list_entry(all->next, aff_prog_t, pl_node);
	pd =  prog->sched_data;
	perf_fair_elem_move2tail(&prog->pl_node, &sched->progs_sch);
	schedule_setcpurange(prog, core, core + prog->cores_needed);
	schedule_reattach(prog);
	pd->cpu = get_cpus(prog);
	sched->monitor_wnd++;
}

void perf_fair_schedule(sched_t *sched){
	aff_prog_t		*prog, *tmp;
	prog_data_t  	*pd;
	int				cores_allocated = 0;
	long long int	bw_rem = BW_LIMIT;
	list_t			*all = &sched->progs_all;
	list_t			*sch = &sched->progs_sch;

	if (monitoring_phase(sched)) {
		monitor(sched);
	} else {
		fprintf(sched->scheduleF, "Choosing progs.....\n");
		qsrt_get_val = perf_fair_get_value;						// sort criteria is prog criteria
		qsrt_quicksort(all, ASC);									// sort all progs

		cds_list_for_each_entry_safe(prog, tmp, &sched->progs_all, pl_node) {	
			if (cores_allocated == MAX_CORES) break;
			pd = prog->sched_data;															// needed for printing

			if (pd->sum_mem_reads > bw_rem - 271250000) continue;  // 46875000 = 3 GB/s, 15625000 = 1 GB/s, 21000000
			bw_rem -= pd->sum_mem_reads;
			if (bw_rem < 0) bw_rem = 0;

			fprintf(sched->scheduleF,"\tPROG: %20s  ON_INSTR: %14llu  IDEAL_INSTR: %14llu  ON_INSTR/IDEAL:%12llu  WAITING TIME: %6d CLASS: %3d BW_REM: %10.8lf BW_ALONE: %10.8lf\n", pd->prg, pd->instr_retd_sum, pd->ideal_instr, pd->ratio, pd->waiting_time, pd->category, (double) (bw_rem * 64) / 1000000.0, (double) (pd->sum_mem_reads * 64) / 1000000.0);
			perf_fair_elem_move2tail(&prog->pl_node, sch);
			schedule_setcpurange(prog, cores_allocated, cores_allocated + prog->cores_needed);
			schedule_reattach(prog);
			pd->cpu = get_cpus(prog);
			cores_allocated += prog->cores_needed;
		}

		if((cores_allocated != MAX_CORES) && (!cds_list_empty(all))){
			if(sched->progs_nr <= MAX_CORES) fill_rest_cores(sched, cores_allocated);
			else {
				while(cores_allocated != MAX_CORES) {
					if ((prog = good_neighbors(sched, bw_rem)) != NULL) {
						pd = prog->sched_data;
						bw_rem -= pd->sum_mem_reads;
						if (bw_rem < 0) bw_rem = 0;
						schedule_setcpurange(prog, cores_allocated, cores_allocated + prog->cores_needed);
						schedule_reattach(prog);
						pd->cpu = get_cpus(prog);
						cores_allocated += prog->cores_needed;
					} else {
						fill_rest_cores(sched, cores_allocated);
						break;
					}
				}
			}
		}
	}
}

void perf_fair_respawn(aff_prog_t *prog){
	prog_data_t  *pd = prog->sched_data;
	sched_t      *sched  = pd->sched;
	int respawn = 0;
	int i = pd->init_id;

	prog->thr_real = pd->ipc_alone;
	prog->instr_retd_sum = pd->instr_retd_sum;
	prog->ideal_instr = pd->ideal_instr;
	prog->sum_mem_reads = pd->sum_mem_reads;
	prog->llc_alone = pd->llc_alone;

	if (sched->target_instr[i] > 1000000000) respawn = 1;

#ifndef TIME_WINDOW
	if (!respawn) sched->progs_left--;
	if (sched->progs_left <= 0) 
	{
		fprintf(stderr, "Terminating...\n");
		ExecSt.massacre 		= 1;
		sched->stop_execution 	= 1;
		perf_fair_freeze_sched(sched);
		perf_fair_kill_all(sched);
		sched_print_totals(sched);
		sched_finalization(sched);
	}
	if (sched->stop_execution) return;
#endif
	if (respawn) prog_respawn(prog, sched->progs_init + prog->id);
}

void perf_fair_rebalance(void *arg){
	sched_t 		*sched = arg;
	aff_prog_t		*prog, *prog_tmp;
	prog_data_t		*pd;
	pid_t			pid;
	__u64			unh_cycles = 0;

	/* handle finished programs */
	cds_list_for_each_entry_safe(prog, prog_tmp, &ExecSt.pfinished_l, pl_node) 
	{
		cds_list_del(&prog->pl_node);
		CDS_INIT_LIST_HEAD(&prog->pl_node);
		printf("Handle finished programs\n");
		pd	= prog->sched_data;

		if ((!pd->killed) && (!sched->stop_execution)) {
			pd	= prog->sched_data;
			ss_aff_stop_counter(sched->cntrs, pd->cpu);
			ss_aff_read_counter(sched->cntrs, pd->cpu);
			stopMCCounters(sched->cntrs);
			stopPower(sched->cntrs);
			get_offcore_metrics(sched);
			ss_aff_counter_start(sched->cntrs, pd->cpu);
			ss_aff_stats_pause(pd->stats, prog->cpuset);
			ss_aff_stats_read(pd->stats, prog->cpuset);
			sched_update_clock(pd);
			sched_update_prfm(prog);
			++pd->times_cpu;
			pd->instr_retd_sum += pd->instr_retd;
			sched_print_counters(prog, sched);
			sched_print_prog_finish(sched, prog);
			unh_cycles = pd->unh_cycles;
			perf_fair_update_progress(pd, unh_cycles);
			sched->target_instr[pd->init_id] -= (long long int) pd->instr_retd;
			sched_update_totals(sched, prog);
			perf_fair_respawn(prog);
		}
		sched->progs_fnsh++;
		sched->progs_nr--;
		sched_del_prog(prog);
	}

	/* handle new programs */
	cds_list_for_each_entry_safe(prog, prog_tmp, &ExecSt.pnew_l, pl_node) 
	{
		// printf("Handle new programs\n");
		if (sched->stop_execution)
		{
			/*
			 Signal handlers are disabled when an NEW_PROG event arrives. 
			 That's why we perform dealloc here. In addition you have to 
			 kill the prog because stop execution is on, meaning that this
			 prog finished at the same time with the last prog.
			*/
			pid = prog->pid;
			kill(pid, SIGKILL);
			ExecSt.new_nr--;
		}
		else
		{
			sched_add_prog(sched, prog);
			sched->progs_nr++;
			pd = prog->sched_data;
			perf_fair_elem_move2tail(&prog->pl_node, &sched->progs_all);
			aff_prog_new_running(prog);
		}
	}
}

void perf_fair_qexpired(void *arg, struct timeval *now)
{
	sched_t 	*sched = arg;

	sched->total_tics++;
	ExecSt.total_quantums++;
	fprintf(ExecSt.fsched, "QUANTUM %d \n", ExecSt.total_quantums); fflush(stdout);
	fprintf(stderr, "total_tics = %d\n", sched->total_tics);
	perf_fair_freeze_sched(sched);			// freeze running programs, update waiting times

#ifdef	TIME_WINDOW
	if ( sched->total_tics < RUN_TICS ) {
#endif	
		perf_fair_schedule(sched);
		perf_fair_thaw_sched(sched);
#ifdef	TIME_WINDOW 
	}else{
		fprintf(stderr, "Terminating...\n");
		ExecSt.massacre 		= 1;
		sched->stop_execution 	= 1;
		perf_fair_kill_all(sched);
		sched_print_totals(sched);
		sched_finalization(sched);
	}
#endif	

	timeradd(&qt, now, &qt_event.tv);
	heap_push(ExecSt.events_heap, (unsigned long)&qt_event);
}

REGISTER_SCHEDULER(perf_fair, perf_fair_init, perf_fair_rebalance, perf_fair_qexpired, NULL);

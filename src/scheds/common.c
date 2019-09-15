#include "../stats.h"
#include "common.h"

/************************ CPUSET MANAGEMENT ***************************/

void get_cpuset_tasks (aff_prog_t *p)
{
	int i, n;
	pid_t spid;
	struct cpuset_pidlist *list; 

	list 	= cpuset_init_pidlist(p->idpath, 1);
	n 		= cpuset_pidlist_length(list);

	for(i = 0; i < n; i++)
	{
		spid = cpuset_get_pidlist(list, i);
		fprintf(stderr, "PID = %d and SPID = %d\n", p->pid, spid);
	}

	cpuset_freepidlist(list);
}

int get_cpus (aff_prog_t *p)
{
	int ret;
	char *bmr_str = aff_prog_cpuset_str(p);
	fprintf(stderr, "prog %d running on [%s]\n", p->pid, bmr_str);
	ret = atoi(bmr_str);

	return ret;
}

/*************************** OUTPUT FILES MANAGEMENT ************************************/

void sched_open_results_files (sched_t *sched)
{
	char err = 0;
	
	if (((sched->finishedF = fopen(RES_DIR "finisheds-out", "w")) == NULL) || 
	   ((sched->countersF = fopen(RES_DIR "counters-out", "w")) == NULL) ||
	   ((sched->scheduleF = fopen(RES_DIR "sched-out", "w")) == NULL) ||
	   ((sched->energyF = fopen(RES_DIR "energy-out", "w")) == NULL)) {
		fprintf(ExecSt.fsched, "OUTPUT FILE ERROR\n");
		err = 1;
	}
	if (err) {
		perror("open:");
		exit(-1);		
	}
}

void sched_close_results_files (sched_t *sched)
{
    fclose(sched->countersF);
    fclose(sched->finishedF);
    fclose(sched->scheduleF);
    fclose(sched->energyF);
    if (sched->targetF) fclose(sched->targetF);
}

void sched_print_prog_finish(sched_t *sched, aff_prog_t *p)
{
	prog_data_t		*pd =  p->sched_data;

	fprintf(ExecSt.fsched, "Normal exit PROGRAM: $%70.70s$  secs: $ %6.0lf $ cpu:$%6.0d $ offcpu:$%6.0d\n ", 
		p->cmd, 1000 * tsc_getsecs(&p->timer), pd->tm_running, pd->tm_waiting);
}

void sched_init_counters_output(sched_t *sched)
{
	FILE *cnoutF = sched->countersF;

	fprintf(sched->energyF,"$Tick\t$Package Energy\t$DRAM Energy\n");
	fprintf(cnoutF, "IRETD:\t\t\tInstructions Retired in Giga\n");
	fprintf(cnoutF, "CYCLES:\t\t\tCPU Cycles in GHz\n");
	fprintf(cnoutF, "LLC_MISSES:\t\tLLC Misses in Kilo\n");
	fprintf(cnoutF, "MPKI:\t\t\tMisses per Kilo Instructions\n");
	fprintf(cnoutF, "MEM_BW(C):\t\tPer Core Memory Bandwidth in MB/s\n");
	fprintf(cnoutF, "MEM_RD BW(S):\tSocket Read Memory Bandwidth in MB/s\n");
	fprintf(cnoutF, "MEM_WR BW(S):\tSocket Write Memory Bandwidth in MB/s\n");
	fprintf(cnoutF, "L3->L2 BW:\t\tL3->L2 Bandwidth in MB/s\n");
	fprintf(cnoutF, "L2->L1 BW:\t\tL2->L1 Bandwidth in MB/s\n");
	fprintf(cnoutF, "IPC:\t\t\tInstructions per Cycles\n");
	fprintf(cnoutF, "L3_OCC:\t\t\tL3 Occupancy in KB\n");
	fprintf(cnoutF, "TEMP(C):\t\tCore Temperature in Celcius degrees (Tjmax = %llu, TCCoffset = %llu)\n", sched->cntrs->TCCActivation, sched->cntrs->TCCOffset);
	fprintf(cnoutF, "TEMP(P):\t\tPackage Temperature in Celcius degrees\n\n");
}

void get_offcore_metrics(sched_t *sched)
{
	offcntrs *offcore = &sched->offcoreCounters;

	offcore->pkgEnergyCounter = getEnergyCounters(sched->cntrs, PKG_ENERGY_COUNTER);
	offcore->dramEnergyCounter = getEnergyCounters(sched->cntrs, DRAM_ENERGY_COUNTER);	
	fprintf(sched->energyF,"$%6d\t$%6.4lf\t$%6.4lf\n", sched->total_tics, offcore->pkgEnergyCounter, offcore->dramEnergyCounter);
	offcore->UncMCReads = sched->cntrs->MCinfo.UncMCNormalReads;
	offcore->UncMCWrites = sched->cntrs->MCinfo.UncMCFullWrites;
}

unsigned int get_pro_execution_metrics(sched_t *sched, int core)
{
	struct timeval tms;
	unsigned int current;

	aff_counters_t *cntrs = sched->cntrs;

	gettimeofday(&tms, NULL);
	current = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);

	current -= sched->sched_start;

	if (ExecSt.DTSAvailable) sched->offcoreCounters.dts_temperature = cntrs->TCCActivation - cntrs->prfcnt[core].DTSInfo.counterRegister;
	if (ExecSt.PLTAvailable) sched->offcoreCounters.plt_temperature = cntrs->TCCActivation - cntrs->prfcnt[core].PLTInfo.counterRegister;

	return current;
}

void print_pro_execution_metrics(sched_t *sched)
{
	FILE *cnoutF = sched->countersF;
	unsigned int time;
	aff_counters_t *cntrs = sched->cntrs;

	offcntrs *offcore = &sched->offcoreCounters;

	time = get_pro_execution_metrics(sched, ExecSt.running_core);

	fprintf (cnoutF,"Running on the core\t=\t%4d\n", cntrs->cpus[ExecSt.running_core]);
	if (ExecSt.DTSAvailable) fprintf (cnoutF,"Core Temperature\t=\t%4llu\n", offcore->dts_temperature);
	if (ExecSt.PLTAvailable) fprintf (cnoutF,"Package Temperature\t=\t%4llu\n", offcore->plt_temperature);

	fprintf (cnoutF,"Package Energy\t\t=\t%6.4lf\n", offcore->pkgEnergyCounter);
	fprintf (cnoutF,"DRAM Energy\t\t\t=\t%6.4lf\n", offcore->dramEnergyCounter);
	fprintf (cnoutF,"Time\t\t\t\t=\t%6d\n\n", time);
}

void sched_initial_metrics(sched_t *sched)
{
	FILE *cnoutF = sched->countersF;

	if (sched->progs_init == 1) print_pro_execution_metrics(sched);

	fprintf(cnoutF, "     $PROG            $PID   $CPU  $TIME       ");
	fprintf(cnoutF, "$IRETD         ");
	fprintf(cnoutF, "$CYCLES       ");
	fprintf(cnoutF, "$LLC_MISSES      ");
	fprintf(cnoutF, "$MPKI       ");
	fprintf(cnoutF, "$MEM BW(C)     ");
	fprintf(cnoutF, "$L3->L2 BW      ");
	fprintf(cnoutF, "$L2->L1 BW       ");
	fprintf(cnoutF, "$IPC        ");
	if (ExecSt.CMTAvailable) fprintf(cnoutF, "$L3_OCC      ");
	if (ExecSt.DTSAvailable) fprintf(cnoutF, "$TEMP(C) ");
	if (ExecSt.PLTAvailable) fprintf(cnoutF, "$TEMP(P)  ");
	fprintf(cnoutF, "$MEM RD BW(S)  ");
	fprintf(cnoutF, "$MEM WR BW(S)  ");
	fprintf(cnoutF, "$PKG ENERGY  ");
	fprintf(cnoutF, "$DRAM ENRGY  ");
	fprintf(cnoutF, "\n");
}

void sched_print_counters (aff_prog_t *p, sched_t *sched)
{
	prog_data_t *data = p->sched_data;
    aff_stats_t *stats = data->stats;
	FILE *cnoutF = sched->countersF;
	offcntrs *offcore = &sched->offcoreCounters;
	unsigned int time_quantum = data->tm_rstop - data->tm_rstart;

	/* add to freeze the function get_offcore_metrics */

	fprintf (cnoutF, "$%20s $%4d $%d  $%6d  ", data->prg, p->pid, stats->cpu, data->tm_running);
	fprintf (cnoutF,"$%13llu  ", data->instr_retd);
	fprintf (cnoutF,"$%13llu  ", data->unh_cycles);
	fprintf (cnoutF,"$%12.3lf  ", (double) (data->llc_misses) / (double) (1000));
	fprintf (cnoutF,"$%11.10lf  ", (double) (data->llc_misses * 1000) / (double) (data->instr_retd));
	fprintf (cnoutF,"$%12.6lf  ", (double) (data->mem_reads * 64) / (double) (time_quantum * 1000));
	fprintf (cnoutF,"$%12.6lf  ", (double) (data->l2_in * 64) / (double) (time_quantum * 1000));
	fprintf (cnoutF,"$%12.6lf  ", (double) (data->l1_repl * 64) / (double) (time_quantum * 1000));
	fprintf (cnoutF,"$%10.6lf  ", (double) data->instr_retd/ (double) data->unh_cycles);

	if (ExecSt.CMTAvailable) fprintf (cnoutF,"$%12.3lf   ", (double) data->llc_occupancy/ (double) 1000);
	if (ExecSt.DTSAvailable) fprintf (cnoutF,"$%4llu    ",  data->dts_temperature);
	if (ExecSt.PLTAvailable) fprintf (cnoutF,"$%4llu     ",  data->plt_temperature);

	fprintf (cnoutF,"$%12.6lf  ", (double) (offcore->UncMCReads * 64) / (double) (time_quantum * 1000));
	fprintf (cnoutF,"$%12.6lf    ", (double) (offcore->UncMCWrites * 64) / (double) (time_quantum * 1000));
	fprintf (cnoutF,"$%6.4lf    ", offcore->pkgEnergyCounter);
	fprintf (cnoutF,"$%6.4lf  ", offcore->dramEnergyCounter);
	fprintf (cnoutF,"\n");
}

void sched_update_prfm(aff_prog_t *prog)
{
	prog_data_t	*pd = prog->sched_data;

	pd->instr_retd		= ss_get_counter(pd->stats, 0, PROG_EVENT);
	pd->unh_cycles		= ss_get_counter(pd->stats, 1, PROG_EVENT);
	pd->llc_misses 		= ss_get_counter(pd->stats, 3, PROG_EVENT);
	pd->l1_repl 		= ss_get_counter(pd->stats, 4, PROG_EVENT);
	pd->l2_in   		= ss_get_counter(pd->stats, 5, PROG_EVENT);
	pd->mem_reads		= ss_get_counter(pd->stats, 6, PROG_EVENT);

	// Extract L3 Occupancy 

	if (ExecSt.CMTAvailable) pd->llc_occupancy = ss_get_counter(pd->stats, 0, RMID_EVENT);
	if (ExecSt.DTSAvailable) pd->dts_temperature = ss_get_counter(pd->stats, 0, TEMP_EVENT);
	if (ExecSt.PLTAvailable) pd->plt_temperature = ss_get_counter(pd->stats, 1, TEMP_EVENT);
}

/***************** TOTAL RESULTS MANAGEMENT *************************/

void sched_update_totals(sched_t *sched, aff_prog_t *prog)
{
	prog_data_t 	*pd = prog->sched_data;
	__u64			total_instr;
	unsigned int 	current;
	int				i;
	struct timeval tms;

	gettimeofday(&tms, NULL);
	current = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);
	current -= sched->sched_start;

	total_instr = get_total_instr(pd->stats);
	i = pd->init_id;
	sched->finish_time[i] = current;
	sched->total_instr[i] += total_instr;
	sched->total_cycles[i] += get_total_cycles(pd->stats);
	if (sched->instr_progs[i] == 0) sched->instr_progs[i] = total_instr;

	fprintf(sched->finishedF, "Finished Prog: %30s Running Time: %6u  Waiting Time: %6u Secs: %6.0lf Scheduling Time: %6u\n", pd->prg, pd->tm_running, pd->tm_waiting, 1000 * tsc_getsecs(&prog->timer), current);

	if (pd->killed) sched->last_instr[i] = total_instr;
	if (!pd->killed) sched->prog_times_finished[i]++;
    sched->total_rtime[i] += pd->tm_running;
	sched->total_wtime[i] += pd->tm_waiting;
	sched->quanta[i] = sched->total_tics;
}

void sched_print_totals(sched_t *sched)
{
	int i,n = sched->progs_init;
	unsigned int total;
	double times;
	struct timeval tms;

    gettimeofday(&tms, NULL);
	total = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);
	total -= sched->sched_start;
	
	fprintf(sched->finishedF, "--------------------------------------------------------------------------------------------------\n");

	for (i = 0; i < n; i++){
		times = (double) sched->prog_times_finished[i] + (double) sched->last_instr[i]/sched->instr_progs[i];
		fprintf(sched->finishedF, "PROG:$ %30s $TIMES FINISHED:$ %4.4f $QUANTUM:$ %6d $FINISH TIME:$ %4.4f $TOTAL RUN TIME:$ %4.4f $TOTAL WAIT TIME:$ %4.4f$\n", sched->total_progs[i], times, sched->quanta[i], (double)sched->finish_time[i]/1000.0, (double)sched->total_rtime[i]/1000.0, (double)sched->total_wtime[i]/1000.0);
	}

	fprintf(sched->finishedF, "TOTAL SHEDULING TIME : %4.4f\n", (double) total/1000.0);
}

/******************************* Clock Management *******************************/

void sched_start_clock(prog_data_t *pd)
{
	struct timeval   tms;

	if ( pd->run_status == RS_RUNNING ) return;			// if running there is nothing to start
	gettimeofday(&tms, NULL);
	pd->tm_wstop 	= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// waiting clock
	pd->tm_rstart 	= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// running clock
	pd->tm_waiting += pd->tm_wstop - pd->tm_wstart;
}

void sched_stop_clock(prog_data_t *pd)
{
	struct timeval   tms;

	if ( pd->run_status == RS_STOPED ) return;			// if is not running is not stoping
	gettimeofday(&tms, NULL);
	pd->tm_rstop    = tms.tv_sec * 1000 + tms.tv_usec / 1000;	// running clock
	pd->tm_wstart 	= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// waiting clock
	pd->tm_running += pd->tm_rstop - pd->tm_rstart;
}

void sched_update_waiting(prog_data_t *pd)
{
	struct timeval	tms;
	int		current;

	gettimeofday(&tms, NULL);
	current 		= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// current time in ms
	pd->waiting_time 	= current - pd->tm_wstart; 			// waiting time in ms
}

void sched_init_clock(prog_data_t *pd)
{
    struct timeval   tms;

    gettimeofday(&tms, NULL);
    pd->tm_wstart 	= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// waiting clock
    pd->tm_wstop 	= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// waiting clock
    pd->tm_rstart 	= tms.tv_sec * 1000 + tms.tv_usec / 1000;	// running clock
    pd->tm_rstop    = tms.tv_sec * 1000 + tms.tv_usec / 1000;	// running clock
	pd->run_status	= RS_STOPED; 
	pd->tm_running	= 0;
	pd->tm_waiting	= 0;
}

void sched_update_clock(prog_data_t *pd)
{
	sched_start_clock(pd);
	sched_stop_clock(pd);
}

/***************** Program Managment *************************/

prog_data_t *sched_prog_alloc(sched_t *sched, aff_prog_t *prog)
{
	prog_data_t		*ret;
	
	if ( (ret = malloc(sizeof(*ret)) ) == NULL ) perrdie("malloc", "");
	prog->sched_data 	= ret;
	ret->prog 	 	= prog;
	ret->sched	 	= sched;				
 	ret->stats	 	= ss_aff_stats_init(sched->cntrs);	// init counters
	ret->cores_needed 	= prog->cores_needed;		
	ret->gang_id	 	= prog->mem_needed;
	ret->category	 	= prog->mem_needed;				
	ret->core_id		= prog->thr_needed;
	ret->fresh			= 1;
	ret->llc_alone		= prog->llc_alone;
	ret->ipc_alone		= prog->thr_real;
	ret->ideal_instr 	= prog->ideal_instr;
	ret->instr_retd_sum = prog->instr_retd_sum;
	ret->ratio			= prog->ratio;
	ret->killed			= 0;
	ret->times_cpu     	= prog->times_cpu;
	ret->sum_mem_reads	= prog->sum_mem_reads;
	ret->mem_reads		= prog->mem_reads;
	ret->avg_mem_reads	= (ret->times_cpu) ? (ret->sum_mem_reads / ret->times_cpu) : 0;
	ret->fitness 		= 0;
	sched_init_clock(ret);								// initialize clock
	return ret;
}

void sched_prog_data_free(aff_prog_t *prog)
{
	prog_data_t  *pd 	= prog->sched_data;

	free(pd->prg);
	prog->sched_data 	= NULL;
	ss_aff_stats_free(pd->stats);
	free(pd);
}

void sched_add_prog(sched_t *sched, aff_prog_t *prog)
{
	prog_data_t  *pd;
	int size,i,size1;

	pd		= sched_prog_alloc(sched, prog);											// allocate and init prog data
	pd->init_id	= ((prog->id - ID_USER_OFFSET) % sched->progs_init); 					// to be used later;

	size1 = strlen(WORK_DIR);
	size = strlen(prog->cmd);
	size = size - size1 + 1;
	if ((i=size-18)) size -= i;
	i = pd->init_id;

	if (sched->total_progs[i] == NULL)
	{
		if (!(sched->total_progs[i] = malloc (size * sizeof(char)))) perrdie("malloc","");
		memcpy(sched->total_progs[i],prog->cmd+size1,size-1);
		memset(sched->total_progs[i]+size-1,'\0',1);
	}
	
	if (!(pd->prg = malloc (size * sizeof(char)))) perrdie("malloc","");
	memcpy(pd->prg, sched->total_progs[i], size-1);
	memset(pd->prg+size-1,'\0',1);

	fprintf(stderr, "New prog: %20s, pid: %7d, AFF_ID: %7d, id: %7d\n", pd->prg, prog->pid, prog->id, pd->init_id);
	// fprintf(stderr, "New prog : %s with id = %d, init_id = %d and pid = %d\n", prog->cmd, pd->init_id, prog->id, prog->pid);

	if (sched->do_respawn) pd->respawned = ((ID_USER_OFFSET + sched->progs_init) <= prog->id) ? 1 : 0;
}

void sched_del_prog(aff_prog_t *prog)
{
	// printf("FUNCTION: %s\n", __func__);
	prog_data_t  *pd     = prog->sched_data;

	if (prog == NULL) return;
	ss_aff_stats_shutdown(pd->stats, prog->cpuset);		// close performance stats
	sched_prog_data_free(prog);							// free scheduler program data
	aff_prog_dealloc(prog);								// free program data
}

/************************ Termination Management ************************/

void sched_finalization (sched_t *sched)
{
	ss_aff_stop_counters(sched->cntrs);
	Pciclose(sched->cntrs);
	ss_close_counters(sched->cntrs);
	ss_aff_counters_free(sched->cntrs);
	sched_close_results_files(sched);
}

void sched_clear_prog(aff_prog_t *prog)
{
	hash_delete(ExecSt.prog_pid_hash, prog->pid);
	tsc_pause(&prog->timer);
	ExecSt.running_nr--;
	ExecSt.finished_nr++;
	// sched_del_prog(prog);
}

void read_instr_from_file(sched_t *sched)
{
	char *buff, buffer[1024];
	int id;
	long long int instr;

	while ((buff = fgets(buffer, sizeof(buffer), sched->targetF)) != NULL) {
		sscanf(buffer, "id: %d, instr: %lld", &id, &instr);
		sched->target_instr[id] = instr;
	}
}

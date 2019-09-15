/*
 * An implementation that lets linux_letters to 
 * do all the scheduling and just takes 
 * action to have a full workload for the 
 * whole execution
 */

#include "linux.h"
#include "common.h"

#define ID_USER_OFFSET 1000
#define TICS 400
// #define TICS 280

void read_times_from_file(linux_sched_t *sched)
{
	char *buff, buffer[1024];
	int id;
	double instr;

	while ((buff = fgets(buffer, sizeof(buffer), sched->targetF)) != NULL) {
		sscanf(buffer, "id: %d, times: %lf", &id, &instr);
		sched->times_target[id] = instr;
	}
}

void * linux_init(void) {
	linux_sched_t *ret;
	int i;
	struct timeval tms;
	char target_path[600], *append = "target_linux";

	ret = calloc(1, sizeof(*ret));
	if (!ret)
	        perrdie("calloc", "");

	print_cpu_characteristics();

	CDS_INIT_LIST_HEAD(&ret->progs);
	ret->cntrs = ss_aff_counters_init(ExecSt.cpus_nr, ExecSt.cpus);
	InitPower(ret->cntrs);
	startPower(ret->cntrs);
	ret->progs_nr = 0;
	ret->progs_init = ret->progs_left = ExecSt.waiting_nr;
	ret->stop_execution = 0;
	ret->total_tics = -1;
	//		ret->pid = ExecSt.pid;

	snprintf(target_path, sizeof(target_path), "%s_%s", instr_file, append);

	if ((ret->targetF = fopen(target_path, "r")) == NULL) {
		perror("open:");
		exit(-1);
	}

	ret->totalsF = fopen(RES_DIR "linux_totals-out", "w");
	ret->energyF = fopen(RES_DIR "energy-out", "w");

	if ((ret->total_rtime = malloc(ret->progs_init * sizeof(double))) == NULL) perrdie ("malloc", "");
	if ((ret->quanta = malloc(ret->progs_init * sizeof(double))) == NULL) perrdie ("malloc", "");
	if ((ret->finish = malloc(ret->progs_init * sizeof(double))) == NULL) perrdie ("malloc", "");
	if ((ret->average_rtime = malloc(ret->progs_init * sizeof(double))) == NULL) perrdie ("malloc", "");
	if ((ret->cur_time = malloc(ret->progs_init * sizeof(double))) == NULL) perrdie ("malloc", "");
	if ((ret->times_target = malloc(ret->progs_init * sizeof(double))) == NULL) perrdie ("malloc", "");
	if ((ret->prog_times_finished = malloc(ret->progs_init * sizeof(int))) == NULL) perrdie ("malloc", "");
	if ((ret->total_progs = malloc(ret->progs_init * sizeof(char *))) == NULL) perrdie ("malloc", "");

	for(i=0; i<ret->progs_init; i++) {
		ret->total_progs[i] = NULL;
		ret->quanta[i] = 0.0;
		ret->finish[i] = 0.0;
		ret->cur_time[i] = 0.0;
		ret->total_rtime[i] = 0.0;
		ret->average_rtime[i] = 0.0;
		ret->times_target[i] = 0.0;
		ret->prog_times_finished[i] = 0;
	}

	read_times_from_file(ret);

	qt.tv_sec = 0;
	qt.tv_usec = 200000;

	qt_event.tv.tv_sec  = qt.tv_sec;
	qt_event.tv.tv_usec = qt.tv_usec;
	heap_push(ExecSt.events_heap, (unsigned long)&qt_event);

	gettimeofday(&tms,NULL);
	ret->sched_time = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);


	return ret;
}

linux_prog_data_t * linux_add_prog (linux_sched_t *sched, aff_prog_t *prog)
{
	linux_prog_data_t *ret;
	int i,size,size1;

	if ((ret = malloc(sizeof(*ret))) == NULL) perrdie ("malloc", "");

	ret->killed = 0;
	ret->sched = sched;

	ret->init_id = ((prog->id - ID_USER_OFFSET) % sched->progs_init);
	i = ret->init_id;

	size1 = strlen(WORK_DIR);
	size = strlen(prog->cmd);
	size = size - size1 + 1;

	if (sched->total_progs[i] == NULL) {
		if (!(sched->total_progs[i] = malloc(size * sizeof(char)))) perrdie("malloc","");
			memcpy(sched->total_progs[i],prog->cmd+size1,size-1);
			memset(sched->total_progs[i]+size-1,'\0',1);
		}
/*
	if (!(ret->prg = malloc (size * sizeof(char)))) perrdie("malloc","");
	memcpy(ret->prg, sched->total_progs[i], size-1);
	memset(ret->prg+size-1,'\0',1);
*/
//	fprintf(sched->totalsF, "New prog : %s , %s with id = %d and init_id = %d\n", ret->prg, prog->cmd, ret->init_id, prog->id);
	fprintf(stderr, "New prog: %20s, id: %6d, init_id: %6d, times: %3.8lf\n", sched->total_progs[i], ret->init_id, prog->id, sched->times_target[i]);
	cds_list_add_tail(&prog->pl_node, &sched->progs);   // add program to scheduling list
	sched->progs_nr++;                  // increase active programs
	
	return ret;
}

void linux_print_totals(linux_sched_t *sched)
{
	struct timeval tms;
	int total_time;
	double times;
	int i, n = sched->progs_init;

	gettimeofday(&tms, NULL);
	total_time = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);
	total_time -= sched->sched_time;

	fprintf(sched->totalsF, "------------------------------------------------------------------------------------------------------------------\n");
	
	for(i=0; i<n; i++) {
		times = (double) sched->prog_times_finished[i];
		times += sched->cur_time[i] / sched->average_rtime[i];
		fprintf(sched->totalsF, "PROG:$ %30s $TIMES FINISHED:$ %7.8f $TOTAL RUN TIME:$ %7.8f $TOTAL WAIT TIME:$ %7.8f $CUR TIME:$ %7.8f $AVERAGE TIME:$ %7.8f $QUANTA:$ %7.8f $FINISH:$ %7.8f\n", sched->total_progs[i], times, sched->total_rtime[i] / 1000.0, (total_time - sched->total_rtime[i]) / 1000.0, sched->cur_time[i] / 1000.0, sched->average_rtime[i] / 1000.0, sched->quanta[i], sched->finish[i] / 1000.0);
	}

	fprintf(sched->totalsF, "TOTAL SCHEDULING TIME: %4.4f\n", (double) total_time / 1000);
	fclose(sched->totalsF);
}

void linux_program_finished (linux_sched_t *sched, aff_prog_t *p)
{
	linux_prog_data_t *pd = p->sched_data;
	int i = pd->init_id, current;
	struct timeval tms;

	gettimeofday(&tms,NULL);
	current = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);
	current -= sched->sched_time;

	clock_getcpuclockid(p->pid, &pd->clockid);
	clock_gettime(pd->clockid, &pd->ts);
	pd->msecs = pd->ts.tv_sec * 1000 + pd->ts.tv_nsec / 1000000;

	sched->quanta[i] = (double) sched->total_tics;
	sched->finish[i] = (double) current;
	++sched->prog_times_finished[i];
	--sched->times_target[i];
	sched->total_rtime[i] += (double) pd->msecs;
	sched->average_rtime[i] = (double) pd->msecs;
	// sched->average_rtime[i] = sched->total_rtime[i] / (double) sched->prog_times_finished[i];
	sched->cur_time[i] = 0.0;

	fprintf(sched->totalsF, "NORMAL EXIT PROG: %27s  RUN TIME: %7.8f, TOTAL TIME: %7.8f, AVERAGE TIME: %7.8f, QUANTA: %7.8f, FINISH: %7.8f\n", sched->total_progs[i], pd->msecs / 1000.0, tsc_getsecs(&p->timer), sched->average_rtime[i] / 1000.0, sched->quanta[i], sched->finish[i] / 1000.0);

}

void linux_rebalance(void *arg)
{
	linux_sched_t *sched = arg;
	linux_prog_data_t *pd;
	aff_prog_t *p, *tmp;
	int i;

	cds_list_for_each_entry_safe(p, tmp, &ExecSt.pnew_l, pl_node) {
		cds_list_del(&p->pl_node);
		p->sched_data = linux_add_prog(sched, p);
		aff_prog_new_running(p);
		frzr_setstate(p->idpath, FRZR_THAWED);
	}

	cds_list_for_each_entry_safe(p, tmp, &ExecSt.pfinished_l, pl_node) {
		cds_list_del(&p->pl_node);
		pd = p->sched_data;
		i = pd->init_id;
		sched->progs_nr--;
		if (!pd->killed) {
			linux_program_finished(sched, p);
			// if (sched->total_rtime[i] >= sched->average_rtime[i] * sched->times_target[i]) sched->progs_left--;
			if (sched->times_target[i] == 0.0) sched->progs_left--;
			else prog_respawn(p, sched->progs_init+p->id);
			if (sched->progs_left <= 0) {
				fprintf(stderr,"Terminating....total tics = %d\n", sched->total_tics);
				linux_print_totals(sched);
				ExecSt.massacre = 1;
				fprintf(stderr, "After print_totals\n");
			}
		}
		aff_prog_dealloc(p);
	}
}

void linux_qexpired(void *arg, struct timeval *now)
{
    linux_sched_t *sched = arg;
    aff_prog_t *p;
	linux_prog_data_t *pd;
	struct timeval tms;
	int current, i;
	double pkgEnergyCounter, dramEnergyCounter;

	gettimeofday(&tms,NULL);
	current = ((tms.tv_sec % 10000) * 1000) + (tms.tv_usec / 1000);

	current -= sched->sched_time; 

	++sched->total_tics;
	fprintf(stderr,"total tics = %d, quantum = %d\n", sched->total_tics, current);

	stopPower(sched->cntrs);
	
	pkgEnergyCounter = getEnergyCounters(sched->cntrs, PKG_ENERGY_COUNTER);
	dramEnergyCounter = getEnergyCounters(sched->cntrs, DRAM_ENERGY_COUNTER);	
	fprintf(sched->energyF,"$%6d\t$%6.4lf\t$%6.4lf\n", sched->total_tics, pkgEnergyCounter, dramEnergyCounter);

	cds_list_for_each_entry(p, &sched->progs, pl_node) {
		pd = p->sched_data;
		i = pd->init_id;
		clock_getcpuclockid(p->pid, &pd->clockid);
		clock_gettime(pd->clockid, &pd->ts);
		pd->msecs = pd->ts.tv_sec * 1000 + pd->ts.tv_nsec / 1000000;
		fprintf(stderr, "ID: %d Time: %f\n", pd->init_id, pd->msecs);
		sched->cur_time[i] = (double) pd->msecs;
		if ((sched->times_target[i] < 1.0) && (sched->cur_time[i] >= sched->average_rtime[i] * sched->times_target[i])) {
			sched->progs_left--;
			sched->quanta[i] = (double) sched->total_tics;
			sched->finish[i] = (double) current;
			sched->total_rtime[i] += (double) pd->msecs;
			pd->killed = 1;
			kill(p->pid, SIGKILL);
			fprintf(sched->totalsF, "KILL PROG: %27s  RUN TIME: %7.8f, TOTAL TIME: %7.8f, AVERAGE TIME: %7.8f, QUANTA: %7.8f, FINISH: %7.8f\n", sched->total_progs[i], pd->msecs / 1000.0, sched->total_rtime[i] / 1000.0, sched->average_rtime[i] / 1000.0, sched->quanta[i], sched->finish[i]);
		}
	}

	if (sched->progs_left <= 0) {
		fprintf(stderr,"Terminating....total tics = %d\n", sched->total_tics);
		linux_print_totals(sched);
		ExecSt.massacre = 1;
		fprintf(stderr, "After print_totals\n");
		fclose(sched->energyF);
	}

	startPower(sched->cntrs);

	timeradd(&qt, now, &qt_event.tv);
	heap_push(ExecSt.events_heap, (unsigned long)&qt_event);
}


REGISTER_SCHEDULER(linux, linux_init, linux_rebalance, linux_qexpired, NULL)


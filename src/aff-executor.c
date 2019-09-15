#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <numa.h>
#include <cpuset.h>
#include <bitmask.h>

#include "list.h"
#include "utils.h"
#include "heap.h"
#include "hash.h"
#include "tsc.h"
#include "freezer.h"
#include "aff_shm.h"
#include "aff-executor.h"

struct exec_state ExecSt = {0};
struct timeval qt = { .tv_sec = 0, .tv_usec = 200000};
aff_event_t qt_event = { .type = EVNT_QEXPIRED };
char instr_file[500];

/* return the string of a bitmask */
char * bitmask_str(struct bitmask *bm)
{
	static char buff[512];
	int ret;

	ret = bitmask_displaylist(buff, sizeof(buff), bm);
	if (ret > sizeof(buff) - 1)
		die("increase buffer size");

	return buff;
}

/* return the string of the cpuset of a aff _prog  */
char * aff_prog_cpuset_str(aff_prog_t *p)
{
	int ret;

	ret = cpuset_getcpus(p->cpuset, ExecSt.tmp_cpu_bm);
	if (ret < 0)
		die("cpuset_getcpus");

	return bitmask_str(ExecSt.tmp_cpu_bm);
}

void aff_prog_print(aff_prog_t *p)
{
	char *bm_str;

	bm_str = aff_prog_cpuset_str(p);
	fprintf(ExecSt.fsched, "id:%d time:%d pid:%d cpuset:[%s] idpath:%s cmd:%s\n",
	        p->id, p->start_time, p->pid, bm_str, p->idpath, p->cmd);
}

void aff_event_print(aff_event_t *evnt)
{
	fprintf(ExecSt.fsched, "evnt: tv=[%lus,%luu] ", evnt->tv.tv_sec, evnt->tv.tv_usec);
	switch (evnt->type) {
		case EVNT_QEXPIRED:
		fprintf(ExecSt.fsched, "QEXPIRED\n");
		break;
		case EVNT_NEWPROG:
		fprintf(ExecSt.fsched, "NEWPROG ");
		aff_prog_print(evnt->prog);
		break;
		case EVNT_SAMPLING:
		fprintf(ExecSt.fsched, "SAMPLING\n");
		break;

		default:
		die("Unknown event type %d\n", evnt->type);
	}
}

/* check exit status of a terminating program */
void aff_prog_check_status(aff_prog_t *p, int status)
{
	if (!WIFEXITED(status)) {
		emsg("prog *not* terminated normally [status:%d]\n", status);
		aff_prog_print(p);
	} else {
		fprintf(ExecSt.fsched, "Normal exit [status:%d] [secs:%lf] [ticks:%lu] ",
		        WEXITSTATUS(status), tsc_getsecs(&p->timer), tsc_getticks(&p->timer));
		aff_prog_print(p);
	}
}

/* WAITING -> NEW state transition */
void aff_prog_waiting_new(aff_prog_t *p)
{
	ExecSt.waiting_nr--;
	cds_list_add_tail(&p->pl_node, &ExecSt.pnew_l);
	ExecSt.new_nr++;
}

/* NEW -> RUNNING state transition */
void aff_prog_new_running(aff_prog_t *p)
{
	ExecSt.new_nr--;
	ExecSt.running_nr++;
	hash_insert(ExecSt.prog_pid_hash, p->shell_pid, (unsigned long)p);  
	fprintf(ExecSt.fsched, "  STARTED RUNNING =>"); aff_prog_print(p);
	tsc_init(&p->timer);
	tsc_start(&p->timer);
}

/* RUNNING -> FINISHED state transition */
void aff_prog_running_finished(pid_t pid, int status)
{
	aff_prog_t *p;
	unsigned long p_ul;

	/* find the program from the hash table */
	p_ul = hash_delete(ExecSt.prog_pid_hash, pid);
	assert(p_ul != HASH_ENTRY_NOTFOUND);
	p = (aff_prog_t *)p_ul;

	tsc_pause(&p->timer);
	aff_prog_check_status(p, status);

	ExecSt.running_nr--;
	cds_list_del(&p->pl_node);
	cds_list_add_tail(&p->pl_node, &ExecSt.pfinished_l);
	ExecSt.finished_nr++;
}

aff_event_t * aff_evnt_alloc(int type)
{
	aff_event_t *ret;

	ret = calloc(1, sizeof(*ret));
	if (!ret)
		perrdie("malloc failed", "");
	assert(type >= EVNT_NEWPROG && type < EVNT_NR);
	ret->type = type;
	return ret;
}

void aff_evnt_dealloc(aff_event_t *evnt)
{
	free(evnt);
}

void aff_event_print_ul(unsigned long evnt_ul)
{
	aff_event_print((aff_event_t *)evnt_ul);
}

/* compare two events, based on their timeval. This is used to maintain the heap */
int aff_event_leq_ul(unsigned long evnt1_ul, unsigned long evnt2_ul)
{
	aff_event_t *evnt1 = (aff_event_t *)evnt1_ul;
	aff_event_t *evnt2 = (aff_event_t *)evnt2_ul;
	return !timercmp(&evnt1->tv, &evnt2->tv, >);
}

/* helper function to change cpuset based on a bitmask */
void do_setcpumask(aff_prog_t *p, struct bitmask *bm)
{
	int ret;
	char buff[64];

	bitmask_displaylist(buff, sizeof(buff), bm);
	ret = cpuset_setcpus(p->cpuset, bm);
	if (ret < 0)
		perrdie("cpuset_setcpus", "");

	ret = cpuset_modify(p->idpath, p->cpuset);
	if (ret < 0)
		perrdie("cpuset_modify", "");
	fprintf(ExecSt.fsched, "%s\n", p->idpath);
}

void schedule_setcpus(aff_prog_t *p, struct bitmask *bm)
{
	int cpus_nr = bitmask_weight(bm);
	do_setcpumask(p, bm);
	aff_shm_set_cpus_assigned(p->shm, cpus_nr);
}

/* helper function to set cpuset, given a range of cpus */
void schedule_setcpurange(aff_prog_t *p, int rstart, int rend)
{
	int *cpus = ExecSt.cpus;
	struct bitmask *bm = ExecSt.tmp_cpu_bm;
	int i;

	bitmask_clearall(bm);
	/* set bitmask */
	for (i=rstart; i<rend; i++) {
		bitmask_setbit(bm, cpus[i]);
	}
	schedule_setcpus(p, bm);
}

/* helper to call cpuset_reattach()
 * From the libcpuset documentation:
 *   It should be noted, however, that changes to the CPUs of a cpuset do not
 *   apply to any task in that cpuset until the task is reattached to that
 *   cpuset (...) to avoid a hook in the hotpath scheduler code in the kernel.
 * So after we set the cpuset, we need to call reattach() for that change to
 * take effect. However, there is a race here, and reattach() may fail. In this
 * case we just shout IMBALANCE!, and do nothing more.
 */
void schedule_reattach(aff_prog_t *p)
{
	int ret;
	ret = cpuset_reattach(p->idpath);
	if (ret < 0){
		perror("cpuset_reattach");
		emsg("pid: %d\n", p->pid);
		emsg("reattach failed: probable IMBALANCE\n");
	}
}

void aff_shm_dealloc(aff_prog_t *prog)
{
	int ret;

	ret = munmap(prog->shm, AFF_SHM_SIZE);
	if (ret == -1)
		perror("munmap");

	ret = shm_unlink(prog->idpath);
	if (ret == -1)
		perror("shm_unlink");
}

/* prepare a shared-memory section for the given program */
void aff_shm_alloc(aff_prog_t *prog)
{
	int fd, res;
	void *shm;
	
	fd = shm_open(prog->idpath, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1)
		perrdie("shm_open", "failed to shm_open(%s)", prog->idpath);

	res = ftruncate(fd, AFF_SHM_SIZE);
	if (res == -1) perrdie("ftruncate" , "failed");

	shm = mmap(NULL, AFF_SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if (shm == MAP_FAILED)
		perrdie("mmap", "failed to mmap for: id=%d", prog->id);
	close(fd);

	/* set default values for shared memory */
	aff_shm_set_fd_executor(shm, ExecSt.pipe[1]);
	aff_shm_set_cpus_arch(shm, ExecSt.cpus_nr);
	aff_shm_set_cpus_assigned(shm, 0);
	/* we use the shell to exec() the program. As a result, we get the pid
	 * of the shell, and not the pid of the program. To distinguish between
	 * different programs sending requests in the pipe, each program needs
	 * to send it's handler via the pipe */
	aff_shm_set_app_handler(shm, (unsigned long)prog);

	prog->shm = shm;
}

/* allocate (and initialize) a structure for a parallel program */
aff_prog_t * aff_prog_alloc(int id)
{
	aff_prog_t *ret;
	int err;

	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		perrdie("malloc failed", "");

	ret->id = id;
	ret->cpuset = cpuset_alloc();
	if (ret->cpuset == NULL)
		perrdie("cpuset_alloc failed", "");

	err = cpuset_setmems(ret->cpuset, ExecSt.mem_bm);
	if (err)
		perrdie("cpuset_setmems", "");

	/* create cpuset for new program */
	snprintf(ret->idpath, AFF_MAX_PATH, "/AFF_%d", ret->id);
	fprintf(ExecSt.fsched, "creating cpuset: /dev/cpuset%s with gid: %d\n", ret->idpath, getgid());
	err = cpuset_create(ret->idpath, ret->cpuset);
	if (err < 0){
		if ((err == ENOENT) || (err == EEXIST)) fprintf(stderr, "Parent cpuset does not exist\n");
		else perrdie("cpuset_create", "");
	}

	fprintf(ExecSt.fsched, "cpuset created\n");
	#ifdef AFF_PRFCNT
	ret->prfcnt = aff_prfcnt_prog_alloc();
	#endif

	fprintf(ExecSt.fsched, "Initializing shared memory of program with id %d\n", ret->id);
	aff_shm_alloc(ret);
	fprintf(ExecSt.fsched, "Shared memory initialized\n");
	return ret;
}

void aff_prog_dealloc(aff_prog_t *p)
{
	int ret;

	cpuset_free(p->cpuset);
	free(p->cmd);
	ret = cpuset_delete(p->idpath);
	if (ret < 0){
		perror("cpu_delete");
		exit(1);
	}

	if (p->pid != 0)
		frzr_shut(p->idpath);

	close(p->pipe[1]);
	aff_shm_dealloc(p);
	free(p);
}

void __attribute__((unused)) sched_self(aff_prog_t *prog)
{
	int ret;

	ret = cpuset_move(0, prog->idpath);
	if (ret < 0)
		perrdie("cpuset_move", "idpath=%s", prog->idpath);
}

void sched_prog(aff_prog_t *prog)
{
	int ret;

	ret = cpuset_move(prog->pid, prog->idpath);
	if (ret < 0)
		perrdie("cpuset_move", "idpath=%s", prog->idpath);
}

/* wrapper to parse an integer */
long parse_int(char *s)
{
	long ret;
	char *endptr;

	ret = strtol(s, &endptr, 10);
	if ( *endptr != '\0' ) {
		fprintf(ExecSt.fsched, "parse error: '%s' is not a number\n", s);
		exit(1);
	}
	return ret;
}

/* helper to parse a list of int's separated by commas */
int __parse_intlist(const char *str, int *ints, int ints_size)
{
	int i, j, temp[2], k;
	char *token, *str1, *str2, *subtoken, *saveptr1, *saveptr2;
	char s[strlen(str) + 1];

	memcpy(s, str, sizeof(s));
	i = 0;

	for (str1 = s; ; str1 = NULL)
	{
		token = strtok_r(str1, ",", &saveptr1);
		if (token == NULL) break;

		for (str2 = token, j = 0; ; str2 = NULL, j++)
		{
			subtoken = strtok_r(str2, "-", &saveptr2);
			if (subtoken == NULL) break;
			temp[j] = (unsigned int)parse_int(subtoken);
		}

		if (j == 1) ints[i++] = (unsigned int)parse_int(token);
		else
		{
			for (k = temp[0]; k <= temp[1]; k++) ints[i++] = (unsigned int) k;
		}
	}

	return i;
}

/* Parse a list of ints, seperated by commas
 * This function allocates memory and returns it, needs to be freed by caller */
int * parse_intlist(const char *str, int max_ints, int *ints_nr_ptr)
{
	int ints[max_ints], ints_nr, *ret, total_bytes;

	ints_nr = __parse_intlist(str, ints, max_ints);
	total_bytes = sizeof(int)*ints_nr;
	ret = malloc(total_bytes);
	if (ret == NULL) {
		fprintf(stderr, "malloc failed");
		assert(0);
	}
	memcpy(ret, ints, total_bytes);

	*ints_nr_ptr = ints_nr;
	return ret;
}

/* parse a (non-comment) line for the configuration file */
void parse_line(char *buff, aff_prog_t *p)
{
	char *tm_str, *prg_str, *cores_str, *mem_str, *thr_str, *tre_str;

	cores_str       = strtok(buff, "\t");
	mem_str         = strtok(NULL, "\t"); /*added by aharit to be used with ss*/
	thr_str         = strtok(NULL, "\t"); /*added by aharit to be used with ss*/
	tre_str         = strtok(NULL, "\t"); /*added by aharit to be used with ss*/

	tm_str		= strtok(NULL, "\t");
	prg_str		= strtok(NULL, "\t");

	if (tm_str == NULL || prg_str == NULL || cores_str == NULL){
		fprintf(stderr, "failed to parse line: -->%s<--\n", buff);
		fprintf(stderr, "(maybe you forgot to use <TAB> for sep?)\n");
		exit(1);
	}

	p->times_cpu = 0;
	p->llc_alone = 0;
	p->sum_mem_reads = 0;
	p->mem_reads = 0;
	p->ratio = 0; 					/*added by tmarin to be used with feliu*/
	p->instr_retd_sum = 0;
	p->ideal_instr = 0;
	p->pid = 0;
	p->start_time = atol(tm_str);
	p->mem_needed = atol(mem_str); /*added by aharit to be used with ss*/
	p->thr_needed = atol(thr_str); /*added by aharit to be used with ss*/
	ExecSt.running_core = p->thr_needed;
	p->thr_real   = atol(tre_str); /*added by aharit to be used with ss*/
	p->cores_needed = atol(cores_str); 
	p->cmd = malloc(strlen(prg_str) + 1);
	if (p->cmd == NULL){
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}
	memcpy(p->cmd, prg_str, strlen(prg_str) + 1);
	//parse_cpusmask(aff_str, &p->affinity);
}

/* redirect stdout/stderr in files in the results_dir for scheduler */
void open_result_file(const char *res_dir)
{
	char fname[strlen(res_dir) + 16];

	snprintf(fname, sizeof(fname), "%s/scaff-out", res_dir);
	ExecSt.fsched = fopen(fname, "w");
}

/* execute a new program helper */
void do_execute(aff_prog_t *prog, const char *res_dir)
{
	pid_t pid;

	/* We use two pipes for communcation with apps:
	 *  - ExecSt.pipe: a single pipe for app->executor communcation
	 *  - prog->pipe: a pipe / app for executor->app communication
	 * The prog->pipe is needed for syncronous communication with
	 * the app. For example, when app notifies the executor that
	 * it enters a new parallel region, it must wait for the executor
	 * to respond with the number of cores assigned to it.
	 */
	if (pipe(prog->pipe) == -1)
		perrdie("pipe", "id:%d", prog->id);

	/* Create a new processes using fork()
	 * A part of the initialization is done by the preload object:
	 * - prepare the shared memory area
	 * - setup * a freezer, and set the state to FROZEN */
	fprintf(ExecSt.fsched, "forking prog: "); aff_prog_print(prog);
	fflush(stdout); fflush(stderr);

	pid = fork();
	fprintf(ExecSt.fsched, "READY to execute:.. after fork with pid: %d \n", pid);fflush(stdout);
	if (pid < 0) {
		perrdie("fork", "id:%d", prog->id);
	} else if (pid == 0) { /* child */
		/* prepare the environment */
		char cmd[1024 + strlen(prog->cmd)];
		snprintf(cmd, sizeof(cmd),
		         " AFF_ID=%d LD_PRELOAD=./aff-executor-preload.so OMP_NUM_THREADS=%d %s",
		          prog->id, prog->cores_needed, prog->cmd);

		/* set-up pipe: reading end is for the app */
		aff_shm_set_fd_app(prog->shm, prog->pipe[0]);
		close(prog->pipe[1]);
		/* ready to execute ... */
		execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
		perror("execl");
		exit(1);
	}

	/* father */
	close(prog->pipe[0]);
	prog->shell_pid = pid;
	/* we don't want to set the program's state to THAWED, before
	 * it has been FROZEN. For this reason, we wait until the
	 * program is FROZEN. This waiting is the reason for which
	 * we fork() programs before they enter the WAITING state,
	 * and not when they enter the RUNNING state */
	while (frzr_getstate(prog->idpath) != FRZR_FROZEN)
		;

	prog->pid = aff_shm_get_app_pid(prog->shm);
	/* we need to create a cpuset dir for this program. Linux does
	 * not allow empty cpusets, so we create a full one */
	schedule_setcpurange(prog, 0, ExecSt.cpus_nr);
	sched_prog(prog);
}


/* parse a file to get the programs */
void parse_file(const char *filename, heap_t *events_heap)
{
	FILE *fp;
	char buffer[1024], *buff;
	int len, cnt;
	aff_event_t *evnt;

	fp = fopen(filename, "r");
	if (fp == NULL)
		perrdie("open", "parse file: %s", filename);

	for (cnt = 1000;; ) {
		buff = fgets(buffer, sizeof(buffer), fp);
		if (buff == NULL)
			break;
		len = strlen(buff);
		if (buff[len-1] != '\n')
			die("line too long\n");
		buff[len-1] = '\0'; /* remove new line */

		if (buff[0] == '#') /* ignore comments */
			continue;
		/* allocate an avent and a program, and parse the line */
		evnt = aff_evnt_alloc(EVNT_NEWPROG);
		evnt->prog = aff_prog_alloc(cnt++);
		parse_line(buff, evnt->prog);
		/* start the execution of the program -- it freezes itself */
		do_execute(evnt->prog, ExecSt.res_dir);
		/* create event for program */
		evnt->tv.tv_sec = evnt->prog->start_time;
		evnt->tv.tv_usec = 0;
		/* push program to the heap */
		heap_push(events_heap, (unsigned long)evnt);
		ExecSt.waiting_nr++;
	}

	fclose(fp);
}


/* return a bitmask describing all the available memory nodes */
struct bitmask * get_mems_all(void)
{
	struct bitmask *ret;

	if (numa_available() != -1){
		fprintf(ExecSt.fsched, "NUMA is available\n");
		return numa_get_mems_allowed();
	}

	ret = bitmask_alloc(cpuset_mems_nbits());
	if (!ret)
		perrdie("bitmask_alloc", "");
	bitmask_setbit(ret,0);
	return ret;
}

/* initialize state */
void init_state(const char *conf_fname, const char *cpus_str)
{
	int ret;

	CDS_INIT_LIST_HEAD(&ExecSt.pnew_l);
	CDS_INIT_LIST_HEAD(&ExecSt.pfinished_l);

	ExecSt.prog_pid_hash = hash_init(MAX_PROGS);
	ExecSt.cpus          = parse_intlist(cpus_str, MAX_PROGS, &ExecSt.cpus_nr);
	ExecSt.mem_bm        = get_mems_all();
	ExecSt.tmp_cpu_bm    = bitmask_alloc(cpuset_cpus_nbits());
	ExecSt.CMTAvailable		= 0;
	ExecSt.MBMLAvailable	= 0;
	ExecSt.MBMTAvailable 	= 0;
	ExecSt.DTSAvailable 	= 0;
	ExecSt.PLTAvailable 	= 0;
	ExecSt.TurboModeON	 	= 0;

	if (!ExecSt.tmp_cpu_bm)
		perrdie("bitmask_alloc", "");

	ret = pipe(ExecSt.pipe);
	if (ret < 0)
		perrdie("pipe", "");

	char buff[128];

	numa_bitmask_clearbit(ExecSt.mem_bm, 1);
	numa_bitmask_setbit(ExecSt.mem_bm,0);
	bitmask_displaylist(buff, sizeof(buff), ExecSt.mem_bm);
	fprintf(ExecSt.fsched, "set memset to %s\n", buff);

	ExecSt.events_heap = heap_init(aff_event_leq_ul, aff_event_print_ul);
	parse_file(conf_fname, ExecSt.events_heap);
	//heap_print(ExecSt.events_heap);
	tsc_init(&ExecSt.timer);
}

/* terminate scheduler */
void shutdown(void)
{
	int exit_val = 0;
	tsc_pause(&ExecSt.timer);
	fprintf(ExecSt.fsched, "THE END: total running time: %lf secs [%lu ticks]\n",
		tsc_getsecs(&ExecSt.timer), tsc_getticks(&ExecSt.timer));
	
	/* if we are in massacre mode -- i.e., we were told to kill
	 * everything and exit, return 42 to let the shell know */
	if (ExecSt.massacre){
		fprintf(ExecSt.fsched, "MASSACRE FINISHED\n");
		exit_val = 42;
	}
	exit(exit_val);
}

/* check if there is nothing more to be done. If not terminate scheudler */
void check_shutdown(void)
{
	if (ExecSt.running_nr == 0 &&
	    ExecSt.waiting_nr == 0 &&
	    ExecSt.new_nr == 0) {
		shutdown();
	}
}

/* Disable delivery of SIGCHLD. */
void signals_disable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}

/* Enable delivery of SIGCHLD. */
void signals_enable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}
}

/* if we get a SIGTERM, start massacre (not yet implemented) */
void sigterm_handler(int signum)
{
	assert(signum == SIGTERM);

	fprintf(ExecSt.fsched, "got a SIGTERM: COMMENCING MASSACRE\n");
	ExecSt.massacre = 1;
}

/* SIGCHILD handler: something happend to a program */
void sigchld_handler(int signum)
{
	pid_t pid;
	int status, deaths;

	deaths = 0;
	assert(signum == SIGCHLD);
	/* iterate over all programs that have finished */
	for (;;){
		pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
		if (pid < 0) {
			perror("waitpid");
			fprintf(stderr, "programs:%d\n", ExecSt.running_nr);
			/*
			 * waitpid() might return: "No child processes"
			 *  (e.g., in the second iteration of the loop, after
			 *   the final program has exited)
			 * so we just print an error
			 */
			if (errno == ECHILD) {
				break;
			}
			exit(1);
		}

		/* nothing more to do ... */
		if (pid == 0) {
			break;
		}

		if (ExecSt.running_nr == 0) {
			emsg("bogus SIGCHILD [pid=%d] with no running progs\n", pid);
			break;
		}

		/* a child has stopped, This is kind of bogus now since we
		 * use the freezer to stop defer program execution */
		if (WIFSTOPPED(status)) {
			emsg("bogus: Child %d stopped\n", pid);
			continue;
		}

		/* a program has been terminated */
		fprintf(ExecSt.fsched, "SIGCHLD for pid=%d\n", pid);
		aff_prog_running_finished(pid, status);
		deaths++;
	}

	if (deaths > 0) {
		ExecSt.sched->rebalance(ExecSt.sched->sched_data);
	}
	check_shutdown();
}

/* Install a signal handler for SIGCHLD.
 * Make sure both signals are masked when one of them is running.
 */
void install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGTERM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}

	sa.sa_handler = sigterm_handler;
	if (sigaction(SIGTERM, &sa, NULL) < 0) {
		perror("sigaction: sigterm");
		exit(1);
	}
}

/* handle an event */
void handle_event(aff_event_t *evnt, struct timeval *now)
{
	assert(evnt);
	switch (evnt->type) {

		case EVNT_NEWPROG: /* new program, ready to be executed */
		/* WAITING -> NEW state transition */
		aff_prog_waiting_new(evnt->prog);
		aff_evnt_dealloc(evnt);
		break;

		case EVNT_SAMPLING:
		#ifdef AFF_PRFCNT
		prfcnt_sample();
		#endif
		break;

		case EVNT_QEXPIRED: { /* quantum expired, run the scheduler hook */
			aff_scheduler_t *sched = ExecSt.sched;
			if (sched->qexpired)
				sched->qexpired(sched->sched_data, now);
		}
		break;

		default:
		die("Unknown event type: %d\n", evnt->type);
	}
}


/*
 * Go through all events in the heap, and select events that are ready.
 * If next_wakeup is not NULL, set it approprietly
 * (if no events exist, leave it untouched)
 */
void select_ready_events(struct timeval *now, struct timeval *next_wakeup)
{
	heap_t *evnt_h;

	evnt_h = ExecSt.events_heap;
	/* select events */
	for (;;) {
		int ret;
		aff_event_t *evnt;

		ret = heap_peek(evnt_h, (void *)&evnt);
		if (!ret) { /* no more events, do nothing */
			fprintf(ExecSt.fsched, "No more events to handle!\n");
			break;
		}

		/* if no prog ready: set up the alarm */
		if ( timercmp(&evnt->tv, now, >) ) {
			if (next_wakeup)
			{
				timersub(&evnt->tv, now, next_wakeup);
			}
			break;
		}

		/* we got a ready event: handle it */
		ret = heap_pop(evnt_h, (void *)&evnt);
		handle_event(evnt, now);
	}
}

void get_now(struct timeval *now)
{
	int ret;
	struct timeval now_abs;

	ret = gettimeofday(&now_abs, NULL);
	if (ret < 0)
		perrdie("gettimeofday", "");
	timersub(&now_abs, &ExecSt.tv0, now);
}

void handle_events(struct timeval *next_wakeup)
{
	struct timeval now;

	get_now(&now);
	select_ready_events(&now, next_wakeup);
	/* new programs ready to run, rebalance */
	if (ExecSt.new_nr > 0) {
		ExecSt.sched->rebalance(ExecSt.sched->sched_data);
	}
}

void handle_prog_notifications(int event_fd) 
{
	aff_scheduler_t *sched = ExecSt.sched;
	void *arg = sched->sched_data;
	static int prog_fds[16];

	/* get all program notifications, and call appropriate callback */
	int rebal = 0, cnt;
	for (cnt = 0; cnt < sizeof(prog_fds); cnt++) {
		unsigned long prog_ul;
		int ret = read(event_fd, &prog_ul, sizeof(unsigned long));

		if (ret < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
			break; /* no more data to read */

		if (ret != sizeof(unsigned long))
			perrdie("read", "read returned: %d\n", ret);

		aff_prog_t *prog = (aff_prog_t *)prog_ul;
		int nr_threads = aff_shm_get_cpus_requested(prog->shm);
		if (sched->prog_changed)
			rebal |= sched->prog_changed(arg, prog, nr_threads);
		prog_fds[cnt] = prog->pipe[1];
	}

	/* rebalance if needed */
	if (rebal) {
		sched->rebalance(arg);
	}

	/* notify programs so that they can continue their execution */
	int x = 0;
	for ( ; cnt > 0; cnt--) {
		int fd = prog_fds[cnt - 1];
		if (write(fd, &x, sizeof(int)) != sizeof(int))
			perrdie("write", "failed to write to fd=%d\n", fd);
	}
}

/*
 * Function to respawn an instance of the program
 * described by p. Its functionality is the same with
 * what does the executor in the beginning of execution 
 * when parsing the configuration file.
 */
void prog_respawn(aff_prog_t *p, int id)
{
	aff_event_t *evnt;
	aff_prog_t *new;
	struct timeval now;
	
	get_now(&now);

	/* Allocate a EVNT_NEWPROG event */
	evnt = aff_evnt_alloc(EVNT_NEWPROG);
	
	/* Allocate and initialize a struct for 
	 * the new program. Make sure the 
	 * AFF_ID does not exist.
	 */
	evnt->prog = aff_prog_alloc(id);
	new = evnt->prog;
	new->pid = 0;
	new->start_time	= now.tv_sec;
	new->cores_needed = p->cores_needed;
	new->mem_needed = p->mem_needed;
	new->thr_needed = p->thr_needed;
	new->thr_real   = p->thr_real;
	new->ratio		= p->ratio;
	new->instr_retd_sum		= p->instr_retd_sum;
	new->ideal_instr		= p->ideal_instr;
	new->times_cpu 			= p->times_cpu;
	new->sum_mem_reads 		= p->sum_mem_reads;
	new->mem_reads 			= p->mem_reads;
	new->llc_alone			= p->llc_alone;
		
	new->cmd 	= strdup(p->cmd); 
	if (new->cmd == NULL) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	/* Begin execution of the new program.
	 * It will freeze itself.
	 */
	do_execute(new, ExecSt.res_dir);

	/*
	 * Set event time and handle it.
	 * (We do not need add the event to 
	 * the heap).
	 */
	evnt->tv.tv_sec = new->start_time;
	evnt->tv.tv_usec = 0;

	heap_push(ExecSt.events_heap, (unsigned long)evnt);
	ExecSt.waiting_nr++;
}

int main(int argc, const char *argv[])
{
	int ret;
	aff_prog_t *p;
	aff_scheduler_t *sched;
	const char *sched_str;
	fd_set rd_set;

	if (argc < 3){
		fprintf(stderr, "Usage: %s <conf_file> <cpus> [<scheduler>]\n", argv[0]);
		exit(1);
	}

	memset(instr_file, '\0', sizeof(instr_file));
	memcpy(instr_file, argv[1], strlen(argv[1]));

	printf("The file with the target instr is: %s\n", instr_file);

	open_result_file("../results");
	init_state(argv[1], argv[2]);

	sched_str = (argc > 3) ? argv[3] : "linux";
	sched = ExecSt.sched = get_scheduler(sched_str);
	fprintf(ExecSt.fsched, "-----| Scheduler: %s CPUS: %s\n", sched->name, argv[2]);
	if (sched->init != NULL)
		sched->sched_data = sched->init();

	install_signal_handlers();
	ret = heap_peek(ExecSt.events_heap, (void *)&p);
	if (!ret) {
		fprintf(stderr, "heap empty!\n");
		exit(1);
	}

	tsc_start(&ExecSt.timer);
	ret = gettimeofday(&ExecSt.tv0, NULL); // this corresponds to t=0 */
	if (ret < 0)
		perrdie("gettimeofday", "");

	int event_fd = ExecSt.pipe[0];
	ret = fcntl(event_fd, F_SETFL, O_NONBLOCK);
	if (ret < 0)
		perrdie("fcntl", "");

	for (;;) {
		struct timeval next_wakeup;
		int ret;

		timerclear(&next_wakeup);

		signals_disable();
		handle_events(&next_wakeup);
		signals_enable();

		do {
			fflush(stdout);
			FD_ZERO(&rd_set);
			FD_SET(event_fd, &rd_set);
			ret = select(event_fd +1, &rd_set, NULL, NULL, &next_wakeup);

			/* If a signal arrives (in our case a SIGCHILD) select()
			 * will return with an EINTR error. This is normal, but
			 * we need to make sure that the next select() sleeps
			 * for the desired amount of time.
			 *
			 * According to select(2) "select()  may  update  the
			 * timeout argument to indicate how much time was left."
			 * Linux implementation seems to do that, so --for now--
			 * there is no need to re-calculate the desired amount
			 * of time to sleep. */
			if (ret < 0 && errno != EINTR) {
				perrdie("select", "");
			} else if (ret > 0) {
				/* we have a notification from an app */
				assert(ret == 1 && FD_ISSET(event_fd, &rd_set));
				handle_prog_notifications(event_fd);
			}

			/* On the other hand, some times (actually most of the
			 * times) we need to reset the timer because a new
			 * schedule started, and we want to give a full tq to
			 * the current gang. The scheduler can set the reset_qt
			 * flag, and this code will take care of it. We could
			 * probably do it unconditionally, but maybe there are
			 * some exceptions (e.g., specific scheduler
			 * implementations) */
			if (ExecSt.reset_qt) {
				printf("Inside RESET_QT\n");
				struct timeval now;
				get_now(&now);
				timeradd(&qt, &now, &qt_event.tv);
				heap_pop_elem(ExecSt.events_heap, (unsigned long)&qt_event);
				heap_push(ExecSt.events_heap, (unsigned long)&qt_event);
			}

		} while (ret != 0); /* no timeout happened */

		/* check if we 've finished */
		check_shutdown();
	}

	return 0;
}

CDS_LIST_HEAD(aff_schedulers);

void register_scheduler(char *name,
                   void *(*init)(void),
                   void (*rebalance)(void *),
                   void (*qexpired)(void *, struct timeval *),
                   int  (*prog_changed)(void *, aff_prog_t *, int))
{
	aff_scheduler_t *sched;

	sched = malloc(sizeof(*sched));
	if (!sched) {
		fprintf(stderr, "malloc failed");
		exit(1);
	}

	sched->init           = init;
	sched->rebalance      = rebalance;
	sched->qexpired       = qexpired;
	sched->prog_changed   = prog_changed;
	sched->name           = name;

	cds_list_add_tail(&sched->node, &aff_schedulers);
}

aff_scheduler_t * get_scheduler(const char *name) {
	aff_scheduler_t *ret;
	cds_list_for_each_entry(ret, &aff_schedulers, node) {
		if (strcmp(name, ret->name) == 0)
			return ret;
	}
	fprintf(stderr, "scheduler %s not found\n", name);
	exit(1);
}



#ifndef __AFF_EXECUTOR_H__
#define __AFF_EXECUTOR_H__

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


typedef struct cds_list_head list_t;
typedef struct cpuset        cpuset_t;
#define MAX_PROGS 128
#define WORK_DIR "../bin/"

/**
 * A (parallel) program scheduled by the executor
 *  The program can be in 4 states (not tracked explicitly):
 *   - WAITING:  waiting in the heap for it's turn to come
 *   - NEW:      ready to be picked up by the scheduler, and executed
 *   - RUNNING:  executing
 *   - FINISHED: program has finished executing
 *
 * As a convenience for the scheduler implementation, the ->pl_node
 * field can be used by the scheduler to enter the node on a list.
 * Note, however, that when the program terminates, it needs to be
 * added to the list with the finished programs, and the generic will
 * perform the following actions:
 *     cds_list_del(&p->pl_node);
 *     cds_list_add_tail(&p->pl_node, &ExecSt.pfinished_l);
 * So, this means that after the scheduler extracts a node from
 * the new list, it should add it to another list, or do a :
 * CDS_INIT_LIST_HEAD()
 * (Is this too complex?)
 */
struct aff_prog {
	int      start_time;           /* time (secs) to start it */
	int      id;                   /* unique identifier */
	char     *cmd;                 /* command to execute */
	list_t   pl_node;              /* node to create linked list of programs */
	pid_t    pid;                  /* pid if prog has been executed, or 0 */
	pid_t	 shell_pid;            /* pid of the shell if prog has been executed, or 0 */
	cpuset_t *cpuset;              /* set of CPUs for this program */
	int	 cores_needed;				 /* cores initially requested for this application */
	tsc_t    timer;                /* timer for execution of this program */
	char     idpath[AFF_MAX_PATH]; /* identifier path (used by freezer/cpuset) */
	void     *sched_data;          /* placeholder to be used by scheduler */
	long	 mem_needed;           /* added by aharit! to be used wtih ss*/
	long	 thr_needed;           /* added by aharit! to be used wtih ss*/
	long	 thr_real;           /* added by aharit! to be used wtih ss*/
	long	 ratio;     		      /* added by tmarin to be used with feliu*/
	unsigned long	 instr_retd_sum;          /* added by tmarin to be used with feliu*/
	unsigned long	 ideal_instr;       	  /* added by tmarin to be used with feliu*/
	unsigned long	 sum_mem_reads;       	  /* added by tmarin to be used with feliu*/
	unsigned long	 mem_reads;       	  /* added by tmarin to be used with feliu*/
	int	 times_cpu;       	  /* added by tmarin to be used with feliu*/
	int	 llc_alone;       	  /* added by tmarin to be used with feliu*/
	char	 *name;
	#ifdef AFF_PRFCNT
	struct   aff_prfcnt_prog *prfcnt;
	#endif
	void     *shm;                   /* shared memory area */
	int      pipe[2];
	int      cores_init;
};
typedef struct aff_prog aff_prog_t;

/**
 * events that are put in the heap
 *  types:
 *   EVNT_NEWPROG:   execute a new program (->prog is the program)
 *   EVNT_QEXPIRED:  Quantum (time-slice) expired
 *   EVNT_SAMPLING: time to do sampling (UNUSED)
 */
struct aff_event {
	enum {EVNT_NEWPROG, EVNT_QEXPIRED, EVNT_SAMPLING, EVNT_NR} type;
	struct timeval tv;         /* time for this event */
	union {                    /* arguments for specific types */
		aff_prog_t  *prog; /* program for EVENT_NEWPROG */
	};
};
typedef struct aff_event aff_event_t;

/**
 * A scheduler implementation:
 * - init() is called to initialize the scheduler.
 *  Whatever init() returns, is stored into sched_data, and passsed to
 *  subsequent invocations of the scheduler
 * - rebalance() is called when one of two things has happened:
 *   1. One or more new programs are ready to be executed
 *     . the programs are in the pnew_l list
 *     . the scheduler (at some point) should remove and start their execution
 *   2. One or more programs have been terminated:
 *     . the programs are in the pfinished_l list
 *     . the scheduler needs to remove them, and call aff_prog_dealloc()
 * - qexpired() is called when a time quantum expired
 * - prog_changed() is called when there is a change in the program
 *   it should return 1, if a rebalance() is needed
 *
 * The scheduler implementation must on itself add QEXPIRED events in the
 * queue, if it wants to impelment time-sharing.
 */
struct aff_scheduler {
	void *(*init)(void);
	void (*rebalance)(void *sched_data);
	void (*qexpired)(void *sched_data, struct timeval *now);
	int  (*prog_changed)(void *sched_data, aff_prog_t *prog, int nr_threads);
	char *name;
	void *sched_data;
	list_t  node;
};
typedef struct aff_scheduler aff_scheduler_t;

/**
 * ExecSt is the global state of the executor
 */
struct exec_state {
	FILE 			*fsched;
	/* heap used to generate events */
	heap_t         *events_heap;
	/* should be handled by the scheduler on ->rebalance() */
	list_t          pnew_l;         /* new programs, ready to execute */
	list_t          pfinished_l;    /* programs that have finished execution */
	/* counters for programs, based on their state */
	int             waiting_nr;     /* are waiting on the queue */
	int             new_nr;         /* need to be spawned */
	int             running_nr;     /* have been spawned */
	int             finished_nr;    /* have finished */

	int             cpus_nr, *cpus; /* number of available cpus, and their ids */
	hash_t          *prog_pid_hash; /* a hash that maps pids to aff_prog structures */
	struct timeval  tv0;            /* This is the timeval that the executor started */
	struct bitmask  *tmp_cpu_bm;    /* temporary bitmask for cpusets */
	struct bitmask  *mem_bm;        /* global bitmask for memory nodes (used by all) */
	const char      *res_dir;       /* dir to store results */
	aff_scheduler_t *sched;         /* scheduler implementation */
	tsc_t           timer;          /* timer to measure execution time */
	int             pipe[2];        /* pipe for communication with children */
	int             massacre:1;     /* kill-all-my-children-and-terminate mode */
	int             reset_qt:1;     /* notify main loop to reset the qt timer */
	
	int 			running_core;
	int 			CMTAvailable:1;
	int 			MBMLAvailable:1;
	int 			MBMTAvailable:1;
	int 			DTSAvailable:1;
	int 			PLTAvailable:1;
	int 			TurboModeON:1;

	unsigned        total_quantums; /* total ticks of execution */
	unsigned        used_cores;     /*total number of cores used in all quantums of execution.*/
};


char *bitmask_str(struct bitmask *bm);
char *aff_prog_cpuset_str(aff_prog_t *p);
void aff_prog_print(aff_prog_t *p);
void aff_event_print(aff_event_t *evnt);
void aff_prog_check_status(aff_prog_t *p, int status);
void aff_prog_waiting_new(aff_prog_t *p);
void aff_prog_new_running(aff_prog_t *p);
void aff_prog_running_finished(pid_t pid, int status);
aff_event_t * aff_evnt_alloc(int type);
void aff_evnt_dealloc(aff_event_t *evnt);
void aff_event_print_ul(unsigned long evnt_ul);
int aff_event_leq_ul(unsigned long evnt1_ul, unsigned long evnt2_ul);
void do_setcpumask(aff_prog_t *p, struct bitmask *bm);
void schedule_setcpus(aff_prog_t *p, struct bitmask *bm);
void schedule_setcpurange(aff_prog_t *p, int rstart, int rend);
void schedule_reattach(aff_prog_t *p);
int partition_iter(int elements, int parts) ;
void aff_shm_dealloc(aff_prog_t *prog);
void aff_shm_alloc(aff_prog_t *prog);
aff_prog_t * aff_prog_alloc(int id);
void aff_prog_dealloc(aff_prog_t *p);
void __attribute__((unused)) sched_self(aff_prog_t *prog);
void sched_prog(aff_prog_t *prog);
long parse_int(char *s);
int  __parse_intlist(const char *str, int *ints, int ints_size);
int * parse_intlist(const char *str, int max_ints, int *ints_nr_ptr);
void parse_line(char *buff, aff_prog_t *p);
void redirect_io_self(const char *res_dir);
void redirect_io(aff_prog_t *prog, const char *res_dir);
void do_execute(aff_prog_t *prog, const char *res_dir);
void parse_file(const char *filename, heap_t *events_heap);
struct bitmask * get_mems_all(void);
void init_state(const char *conf_fname, const char *cpus_str);
void shutdown(void);
void check_shutdown(void);
void signals_disable(void);
void signals_enable(void);
void sigterm_handler(int signum);
void sigchld_handler(int signum);
void install_signal_handlers(void);
void handle_event(aff_event_t *evnt, struct timeval *now);
void select_ready_events(struct timeval *now, struct timeval *next_wakeup);
void get_now(struct timeval *now);
void handle_events(struct timeval *next_wakeup);
void handle_prog_notifications(int event_fd);
void prog_respawn(aff_prog_t *p, int id);
aff_scheduler_t * get_scheduler(const char *name);
void register_scheduler(char *name, void *(*init)(void), void (*rebalance)(void *), void (*qexpired)(void *, struct timeval *), int  (*prog_changed)(void *, aff_prog_t *, int));


extern struct exec_state ExecSt;
/* quantum, hardcoded to 1 sec for now */
extern struct timeval qt;
extern aff_event_t qt_event;
extern char instr_file[];


#define REGISTER_SCHEDULER(name_, init_, rebalance_, qexpired_, prog_changed_)    \
void __attribute__((constructor)) name_ ## init__ (void)                          \
{                                                                                 \
        register_scheduler( #name_, init_, rebalance_, qexpired_, prog_changed_); \
}






#endif

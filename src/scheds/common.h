#ifndef __COMMON_H__
#define __COMMON_H__

#include "../aff-executor.h"

#define RS_STOPED	0
#define RS_RUNNING	1
#define ID_USER_OFFSET  1000
#define MAX_CORES	6
#define WORK_DIR "../bin/"
#define RES_DIR "../results/"

enum class_type{NONE, BW, CS, CNS,	N};

struct gang {
	int 		id;
	int 		progs_nr;
	int 		cores_nr;
	int 		*cores;
	list_t 		progs;
	list_t 		gang_node;
};

typedef struct gang gang_t;

typedef struct {
	double 		pkgEnergyCounter;
	double 		dramEnergyCounter;	
	__u64		UncMCReads;
	__u64		UncMCWrites;
	// updated only for the initial metrics of one active program
	__u64 		dts_temperature;
	__u64 		plt_temperature;
}offcntrs;

struct sched {
// data used for cgang scheduler 
	list_t				gangs;			// all gangs 
	gang_t				*current; 		// active gang
	int 				gangs_nr;
// data used for fop scheduler
	list_t				progs_all;		// all programs 
	list_t				progs_sch;		// programs to be scheduled next quantum
	__u64 				unh_cycles;	
// general scheduling data
	int					total_tics;
	int					monitor_wnd; 
	int					progs_nr;			// total progs number
	int					progs_init;			// number of first programs (not the ones respawned)
	int 				progs_left;			// first programs that are not terminated 
	int					progs_fnsh;			// number of programs finished
	char				stop_execution;		// execution is stoping
	char				do_respawn;			// 
	char				target_exists;			// instructions file 
// counters
	offcntrs 			offcoreCounters;
	aff_counters_t		*cntrs;
// totals
	char 				**total_progs;
	__u64 				*instr_progs;
	__u64				*total_instr;
	__u64 				*total_cycles;
	__u64 				*last_instr;
	long long int		*target_instr;
	__u64 				average_bandwidth;
	int 				*prog_times_finished;
	int 				*quanta;
	unsigned int 		*finish_time;
	unsigned int 		*total_rtime;
	unsigned int 		*total_wtime;
	unsigned int 		sched_start;
// output files	
	FILE				*finishedF;
	FILE				*countersF;
	FILE				*totalsF;
	FILE				*scheduleF;
	FILE				*targetF;
	FILE				*energyF;
};
typedef struct sched sched_t;

struct prog_data{
	aff_prog_t 			*prog;			// main program data
	sched_t				*sched;			// scheduler data
// data used for cgang scheduler;
	gang_t 				*gang;
	int 				gang_id;
	int 				core_id;
	int 				fresh;
	int 				killed;
// data used for fop scheduling
	int					ipc_alone;			// ipc alone execution
	int					llc_alone;			// llc occupancy alone execution
	__u64				instr_retd_sum;		// I1+I2+...+Im,  m = times on cpu
	__u64				ideal_instr;		// N*Iideal
	__u64				ratio;				// I1+...+Im / N*Iideal
	enum class_type 	category;
// data used in scaff
	int					waiting_time;		// current waiting time
	int 				cpu;				// running on cpu
	int 				times_cpu;				// times on cpu
	int 				cores_needed;
	char				respawned;		// istance is respawned
	int					init_id;		
	char				*prg;
// clock
	char				run_status;			// 0 == waiting, 1 == running
	unsigned int		tm_wstart;
	unsigned int		tm_wstop;
	unsigned int		tm_rstart;
	unsigned int		tm_rstop;
	unsigned int		tm_running; 
	unsigned int		tm_waiting;
// counters
	aff_stats_t 		*stats;
	__u64				instr_retd;
	__u64				unh_cycles;
	__u64				llc_misses;
	__u64				llc_misses_unc;
	__u64				l1_repl;
	__u64				l1_evict;
	__u64				l2_in;
	__u64				l2_out;
	__u64				mem_requests;
	__u64				mem_reads;
	__u64				sum_mem_reads;
	__u64				prev_mem_reads;
	__u64				avg_mem_reads;
	__u64				fitness;
	__u64				mem_writes;
	__u64				l3_in;
	__u64				l3_out;
	__u64				llc_occupancy;
	__u64				dts_temperature;
	__u64				plt_temperature;
};
typedef struct prog_data prog_data_t;

void get_cpuset_tasks (aff_prog_t *p);
int get_cpus (aff_prog_t *p);
void sched_open_results_files (sched_t *sched);
void sched_close_results_files (sched_t *sched);
void sched_print_prog_finish(sched_t *sched, aff_prog_t *p);
void sched_init_counters_output(sched_t *sched);
void get_offcore_metrics(sched_t *sched);
unsigned int get_pro_execution_metrics(sched_t *sched, int core);
void print_pro_execution_metrics(sched_t *sched);
void sched_initial_metrics(sched_t *sched);
void sched_print_counters (aff_prog_t *p, sched_t *sched);
void sched_update_prfm(aff_prog_t *prog);
void sched_start_clock(prog_data_t *pd);
void sched_stop_clock(prog_data_t *pd);
void sched_update_waiting(prog_data_t *pd);
void sched_init_clock(prog_data_t *pd);
void sched_update_clock(prog_data_t *pd);
prog_data_t *sched_prog_alloc(sched_t *sched, aff_prog_t *prog);
void sched_prog_data_free(aff_prog_t *prog);
void sched_add_prog(sched_t *sched, aff_prog_t *prog);
void sched_del_prog(aff_prog_t *prog);
void sched_update_totals(sched_t *sched, aff_prog_t *prog);
void sched_print_totals(sched_t *sched);
void sched_finalization (sched_t *sched);
void sched_clear_prog(aff_prog_t *prog);
void read_instr_from_file(sched_t *sched);

#endif
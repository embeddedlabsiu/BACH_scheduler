#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "utils.h"
#include "aff_shm.h"
#include "freezer.h"
//#include "aff-executor.h"

/* This file contains application-side code for hooks, etc */

/* set the number of threads */
#include <omp.h>
static void
set_num_threads(int num_threads)
{
	omp_set_num_threads(num_threads);
//	fprintf(ExecSt.fsched, "Will execute next parallel region with %d\n", num_threads);
}

static void affhooks_init(void) __attribute__((constructor));
static void affhooks_shut(void) __attribute__((destructor));

/* global state */
static struct {
	int  id;
	int  fd_shm, fd_wr, fd_rd;
	int  cpus_arch;
	void *shm;
	unsigned long myhandle;
	char idpath[AFF_MAX_PATH];
} xSt = {0};


/* Hook to notify executor that something has changed
 *  id:          identifier for the region
 *  app_threads: (maximum) number of threads requested by the application
 *               A value of 0 means the appliction does not know
 */
void
affhook_region_notify(int id, int nr_threads)
{
	/* we are entering a new parallel region */
	int ret, x;

	/* set requested threads */
	if (nr_threads == 0 || nr_threads > xSt.cpus_arch)
		nr_threads = xSt.cpus_arch;
	aff_shm_set_cpus_requested(xSt.shm, nr_threads);

	/* notify and wait for scheduler
	   (maybe this can be avoided for some cases) */
	ret = write(xSt.fd_wr, &xSt.myhandle, sizeof(unsigned long));
	if (ret < 0)
		perrdie("write", "");
	ret = read(xSt.fd_rd, &x, sizeof(int));
	if (ret < 0)
		perrdie("read", "");

	/* scheduler made a decision, update */
	nr_threads = aff_shm_get_cpus_assigned(xSt.shm);
	set_num_threads(nr_threads);
	aff_shm_set_cpus_requested(xSt.shm, nr_threads);
}

static void
prepare_shm(char *idpath)
{
	int fd;
	void *shm;

	/* file should already have been created by the executor */
	fd = shm_open(idpath, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1)
		perrdie("shm_open", "failed to shm_open(%s)", idpath);

	shm = mmap(NULL, AFF_SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if (shm == MAP_FAILED)
		perrdie("mmap", "failed to mmap for: idpath=%s", idpath);

	xSt.fd_shm = fd;
	xSt.shm = shm;
}

static void
affhooks_init(void)
{
	char *id_str;
	int id;
	fflush(stdout);

	/* get id from enviroment variable */
	if ((id_str = getenv("AFF_ID")) == NULL)
		die("AFF_ID is not set\n");
	if ((id = atol(id_str)) == 0)
		die("AFF_ID is 0 or not a number\n");

	/* set id, idpath, and pid */
	xSt.id = id;
	snprintf(xSt.idpath, AFF_MAX_PATH, "/AFF_%d", id);

	/* prepare shm, and set/get variables */
	prepare_shm(xSt.idpath);
	xSt.cpus_arch = aff_shm_get_cpus_arch(xSt.shm);
	xSt.fd_rd     = aff_shm_get_fd_app(xSt.shm);
	xSt.fd_wr     = aff_shm_get_fd_executor(xSt.shm);
	xSt.myhandle  = aff_shm_get_app_handler(xSt.shm);
	aff_shm_set_cpus_requested(xSt.shm, 1);
	aff_shm_set_app_pid(xSt.shm, getpid());

	/* freeze */
	frzr_init(getpid(), xSt.idpath);
	frzr_setstate(xSt.idpath, FRZR_FROZEN);
}

static void
affhooks_shut(void)
{
//	fprintf(ExecSt.fsched, "%s:%s()\n", __FILE__, __FUNCTION__);
}

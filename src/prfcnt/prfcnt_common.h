/*
 * prfcnt_common.h -- some common functions
 *
 * Copyright (C) 2007-2011, Computing Systems Laboratory (CSLab), NTUA
 * Copyright (C) 2007-2011, Kornilios Kourtis
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */
#ifndef __PRFCNT_COMMON_H__
#define __PRFCNT_COMMON_H__

/*
 * prfcnt_common.h
 *
 * Common functions (at least for x86 like insruction sets)
 * for accessing performance counters via :
 *  - /dev/msr linux device
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

extern __off64_t lseek64 (int __fd, __off64_t __offset, int __whence);

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/types.h> /* __u32, __u64 */

static inline int prfcnt_wrmsr(int msr_fd, unsigned int addr, __u64 val)
{
	off_t ret;

	ret = lseek64(msr_fd, addr, SEEK_SET);
	if ( ret == (off_t)-1){
		perror("lseek");
		exit(1);
	}

	ret = write(msr_fd, &val, 8);
	if ( ret != 8 ){
		perror("write");
		exit(1);
	}

	return 0;
}

static inline int prfcnt_rdmsr(int msr_fd, unsigned int addr, __u64 *val)
{
	off_t ret;

	ret = lseek64(msr_fd, addr, SEEK_SET);
	if ( ret == (off_t)-1){
		perror("lseek");
		exit(1);
	}

	ret = read(msr_fd, val, 8);
	if ( ret != 8 ){
		perror("read");
		exit(1);
	}

	return 0;
}

static inline int __prfcnt_init(int cpu, int *msr_fd)
{
	int  fd;
	char msr_dev[32];
	snprintf(msr_dev, 32, "/dev/cpu/%d/msr", cpu);

	fd = open(msr_dev, O_RDWR);
	if (fd < 0){
		perror(msr_dev);
		exit(1);
	}

	*msr_fd = fd;

	return 0;
}

static inline void __prfcnt_shut(int msr_fd)
{
	close(msr_fd);
}

#endif

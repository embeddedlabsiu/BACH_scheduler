.PHONY: all clean

CC=gcc

all: aff-executor aff-executor-preload.so intel_technologies

CFLAGS          = -Wall -O2 -std=gnu99 -g
LDFLAGS         = -lpthread -lcpuset -lbitmask -lnuma -lm -lrt
CFLAGS         += -Iprfcnt -DCPU_MHZ_SH=\"./cpu_mhz.sh\"
CFLAGS         += -DCACHE_LINE_SIZE=64

SCHEDS         = linux.o perf.o fair.o perf_fair.o fair_bw_cache.o 
UTILS          = c_qsort.o stats.o common.o

intel_technologies: intel_technologies.c
	$(CC) $< -o $@ 

aff-executor: aff-executor.o heap.o dynarray.o hash.o freezer.o $(UTILS) $(SCHEDS)  
	$(CC)  $^ -o $@ $(LDFLAGS)

aff-executor.o: aff-executor.c
	$(CC) $(CFLAGS) $< -c

freezer.o: freezer.c
	$(CC) $(CFLAGS) $< -c

schedulers: $(SCHEDS)

linux.o: scheds/linux.c
	$(CC) $(CFLAGS) $< -c

perf.o: scheds/perf.c
	$(CC) $(CFLAGS) $< -c

fair.o: scheds/fair.c
	$(CC) $(CFLAGS) $< -c

perf_fair.o: scheds/perf_fair.c
	$(CC) $(CFLAGS) $< -c

fair_bw_cache.o: scheds/fair_bw_cache.c
	$(CC) $(CFLAGS) $< -c

utilities: $(UTILS)

c_qsort.o: scheds/c_qsort.c
	$(CC) $(CFLAGS) $< -c

stats.o: stats.c
	$(CC) $(CFLAGS) $< -c

common.o: scheds/common.c
	$(CC) $(CFLAGS) $< -c

aff-executor-preload.so: freezer.c aff-executor-preload.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -lrt -ldl -o $@
	ln -s $@ libpreload.so

dynarray.o: dynarray.c
	$(CC) $(CFLAGS) $< -c

heap.o: heap.c
	$(CC) $(CFLAGS) $< -c

hash.o: hash.c
	$(CC) $(CFLAGS) $< -c

clean:
	rm -f intel_technologies aff-executor aff-test-inheritance *.o *.so omp/*.so

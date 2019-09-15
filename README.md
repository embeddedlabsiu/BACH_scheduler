# Bandwidth and cache aware scheduler

A run-time system that orchestrates the execution of multi-threaded applications by incorporating user-defined scheduling policies, one of which is our bandwidth and cache aware scheduler. It operates in user-space on top of Linux-based operating systems.

## Prerequisites

Install libraries (Ubuntu)

```bash
sudo apt-get install libnuma-dev libnuma1
sudo apt-get install alien
cd rpm_build/
sudo alien -i *.rpm
cd ../
```

Install libraries (CentOS)

```bash
sudo yum install numactl numactl-devel numactl-libs numad
cd rpm_build/
sudo rpm -i *.rpm
cd ../
```

## Installing

Jump to src/ directory and build the tool

```bash
cd src/
make
```

Load msr module and mount the cpuset/freezer subsystems (one-time run after boot)

```bash
./prepare.sh
```
## Running

-First argument: the configuration file where the workload is defined (check ../conf/ for more mixes)
-Second argument: the set of cores we want to dedicate to this workload (lscpu for more info)
-Third argument: the scheduling policy utilized to control the execution (options: linux, perf, fair, perf_fair, fair_bw_cache)

```bash
./aff-executor ../conf/cache1 0-5 linux
```
Clean cpuset and freezer after each scaff run

```bash
./cleanup.sh
```

## Collecting Results

Jump to the results folder (finisheds-out contains concise execution info about each application in the workload)

```bash
cd ../results/
less finisheds-out
```

## Notes

This tool has been tested on a Intel(R) Xeon(R) CPU E5-2620 v3 @ 2.40GHz server (24 cores in total) that supports Cache Monitoring Technology (CMT).
The operating system run on this server is CentOS Linux release 7.4.1708. One numa node was utilized with hyper-threading disabled (cores 0-5). All the applications are single-threaded. In core architectures performance monitoring capabilities are more limited. Run intel_technologies in src/ directory to check what is supported in your system. If CMT is not supported the scheduler "fair_bw_cache" will not yield the expected performance.

This tool records the aggregate socket bandwidth along with the per-core bandwidth. The aggregate bandwidth is extracted via reading the IMC registers from the PCI. In order to run in a different system one should check the PCI bus.device.function and change the SOCKET0_BUS, MC0_CHANNEL0_DEV_ADDR, MC0_CHANNEL0_FUNC_ADDR values in "prfcntr_xeon_types.h".
Otherwise aggregate socket bandwidth should be disabled by commenting the InitPower(), startPower(), startMCCounters(), stopMCCounters() functions in the scheduler file.

# Acknowledgements

We would like to thank the following contributors for their help and their advices:

- [Kornilios Kourtis](mailto:kkourt@kkourt.io)
- [Nikolas Ioannou](mailto:nio@zurich.ibm.com)
- [Hugh Leather](mailto:hleather@inf.ed.ac.uk)
- [Georgios Goumas](mailto:goumas@cslab.ntua.gr)

Additionally, the following research works have been published on previous versions of the software:

- Marinakis, Theodoros, et al. "An efficient and fair scheduling policy for multiprocessor platforms." 2017 IEEE International Symposium on Circuits and Systems (ISCAS). IEEE, 2017. [paper](https://ieeexplore.ieee.org/document/8050758)
- Haritatos, Alexandros-Herodotos, et al. "LCA: A memory link and cache-aware co-scheduling approach for CMPs." 2014 23rd International Conference on Parallel Architecture and Compilation Techniques (PACT). IEEE, 2014. [paper](https://ieeexplore.ieee.org/abstract/document/7855923)
- Haritatos, Alexandros-Herodotos, et al. "A resource-centric application classification approach." Proceedings of the 1st COSH Workshop on Co-Scheduling of HPC Applications. 2016. [paper](https://mediatum.ub.tum.de/?id=1286948)
- Haritatos, Alexandros-Herodotos, et al. "Contention-Aware Scheduling Policies for Fairness and Throughput." Co-Scheduling of HPC Applications (extended versions of all papers from COSH@HiPEAC 2016). [paper](http://ebooks.iospress.nl/publication/45982)

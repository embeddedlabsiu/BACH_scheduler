#!/bin/bash
# intialize stuff -- run as root

set -e

modprobe msr

chown $USER /dev/cpu/*/msr
chmod 775 /dev/cpu/*/msr

mkdir ../results/
mkdir -p /dev/cpuset
mount -t cpuset cpuset /dev/cpuset
chown $USER /dev/cpuset
chmod 775 /dev/cpuset

mkdir -p /dev/freezer
mount -t cgroup -ofreezer freezer  /dev/freezer
chown $USER /dev/freezer
chmod 775 /dev/freezer

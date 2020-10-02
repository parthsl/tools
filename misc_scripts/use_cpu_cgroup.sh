#! /bin/bash

mkdir grp1;
mkdir -p /sys/fs/cgroup/cpu/grp1;
echo $$ > /sys/fs/cgroup/cpu/grp1/cgroup.procs;
$@
rm -rf /sys/fs/cgroup/cpu/grp1 

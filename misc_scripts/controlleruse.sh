#! /bin/bash

# Provide a script to create a cgroup
mkdir temp;
mkdir -p /sys/fs/cgroup/cpu/temp;
# Will spawn a container now
echo $$ > /sys/fs/cgroup/cpu/temp/cgroup.procs;
$@
rm -rf /sys/fs/cgroup/cpu/temp 

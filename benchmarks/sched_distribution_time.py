#! /usr/bin/env python
'''
This program uses perf sched script/ftrace to track the per thread cpus
The basic function of this script is to find the load distribution time of 
a workload, i.e., the load balancing time by the scheduler to distribute the workload
across multiple cpus.

The script intends to be used by multi-threaded workload which the scheduler distributes
to individual cpus in few time, the time which is calculated by this script.

The dependencies include the proper working of perf sched script and to generate perf sched record data file which is fed here to the script.
'''
import subprocess as sb
import re

def get_params_from_wakeup_trace(line):
    timestamp = 0
    pid = 0
    cpu = 0
    matchObj = re.match(r'(.*) ([0-9]+.[0-9]+): (.*)sched_wakeup(.*):([0-9]+)(.*) CPU:([0-9]+)', line, re.M)
    if matchObj:
        #print(matchObj.group())
        timestamp = matchObj.group(2)
        pid = matchObj.group(5)
        cpu = matchObj.group(7)

    return timestamp, pid, cpu

def get_params_from_migrate_trace(line):
    timestamp = 0
    pid = 0
    cpu = 0
    matchObj = re.match(r'(.*) ([0-9]+.[0-9]+): (.*)sched_migrate_task(.*) pid=([0-9]+) (.*) dest_cpu=([0-9]+)(.*)', line, re.M)
    if matchObj:
        #print(matchObj.group())
        timestamp = matchObj.group(2)
        pid = matchObj.group(5)
        cpu = matchObj.group(7)

    return timestamp, pid, cpu

def get_params_from_proc_fork(line):
    timestamp = 0
    pid = 0
    cpu = -1
    forker = -1
    matchObj = re.match(r'(.*) ([0-9]+.[0-9]+): (.*)sched_process_fork(.*)pid=([0-9]+)(.*) child_pid=([0-9]+)(.*)', line, re.M)
    if matchObj:
        timestamp = matchObj.group(2)
        pid = matchObj.group(7)
        forker = matchObj.group(5)

    return timestamp, pid, cpu, forker


# Regex parsing func over
#Main program stars from here

#Main data structs
current_timestamp = -1
init_timestamp = -1
minimum_creation_time = -1
maximum_distribution_time = 999999.9

class mapvalue:
    def __init__(self, timestamp, cpu):
        self.add_data(timestamp, cpu)
        self.creation_time = timestamp

    def add_data(self, timestamp, cpu):
        self.timestamp = timestamp
        self.cpu = int(cpu)

tracking = dict('')#a hashmap with key as pid and value as object of mapvalue class

allowed_forker = []#list of pids to be tracked
allowed_forker = allowed_forker + [raw_input('Enter pid of workload: ')]
distribution_time = []
#ends ds

process = sb.Popen("sudo perf sched script".split(" "), stdout=sb.PIPE)
script, err = process.communicate()

ncpus, err = sb.Popen("nproc", stdout=sb.PIPE).communicate()
ncpus = int(ncpus)

script = script.split("\n")
for i in script:
    pid = -1
    timestamp = -1
    cpu = -1
    add_value = 0
    if("sched_wakeup" in i):
        timestamp, pid, cpu = get_params_from_wakeup_trace(i)
        add_value = 1
        #print(timestamp, pid, cpu)


    elif("sched_migrate_task" in i):
        timestamp, pid, cpu = get_params_from_migrate_trace(i)
        add_value = 1
        #print(timestamp, pid, cpu)

    elif("sched_process_fork" in i):
        timestamp, pid, cpu, forker = get_params_from_proc_fork(i)
        if(forker in allowed_forker):
            add_value = 1
            allowed_forker += [pid]

    if add_value==1:
        if pid in tracking:
            if(tracking[pid].cpu == -1):
                if tracking[pid].creation_time  > minimum_creation_time:
                    minimum_creation_time = tracking[pid].creation_time#Sets time when all workload threads are forked
                    maximum_distribution_time = timestamp #Set distribution time at atleast this time
            tracking[pid].add_data(timestamp,cpu)
        else:
            tracking[pid] = mapvalue(timestamp, cpu)

        current_timestamp = timestamp
        
        #check if all pids are on different cpus
        total_pids = len(allowed_forker)
        cpus_covered = [0 for _ in range(ncpus)]

        for i in allowed_forker:
            if i in tracking:
                cpus_covered[tracking[i].cpu] = 1

        cpus_not_covered_counter = 0
        for i in cpus_covered:
            if i==0:
                cpus_not_covered_counter += 1

        if cpus_not_covered_counter==0 or (ncpus > total_pids and cpus_not_covered_counter==ncpus-total_pids):

            if float(maximum_distribution_time) > float(current_timestamp):
                maximum_distribution_time = current_timestamp
            distTime = float(maximum_distribution_time)-float(minimum_creation_time)
            if distTime not in distribution_time:
                distribution_time += [distTime]

print("Distribution time = ", distribution_time[1:])
print("Final thread settlement when workload ends")
print("PID\tCPU")
filter_pids = allowed_forker
for i in filter_pids:
    if i in tracking:
        print(i + "\t" + tracking[i].timestamp + "\t" + str(tracking[i].cpu))

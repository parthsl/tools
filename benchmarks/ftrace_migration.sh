#!/bin/bash
# This script enable migration_task event and prints log related to it
# in trace file. 
# @arg1 : set=1, reset=0
# @arg2 : trace filter. e.g "pid == `pidof a.out` || comm == a.out"
trace="/sys/kernel/debug/tracing/";
tracing_on="/sys/kernel/debug/tracing/tracing_on";
events="/sys/kernel/debug/tracing/events/sched/sched_migrate_task/";
arg1=$1;
arg2=$2;

echo $arg1,$arg2;

enable_trace () {
	echo "nop" > $trace/current_tracer;
	echo 0 > $trace/events/enable;
	echo 1 > $events/enable;
	echo "$arg2" > $events/filter;
	echo > $trace/trace;
	echo 1 > $tracing_on;
}

disable_trace () {
	echo 0 > $tracing_on;
	echo 0 > $trace/events/enable;
	echo 0 > $events/filter;
	echo 0 > $events/enable;
}

if [ $arg1 -eq 1 ]
then
	enable_trace
else
       	if [ $arg1 -eq 0 ]
	then
		disable_trace
	else
		if [ $arg1 -eq 2 ]
		then
			cp $trace/trace ./new_trace;
		fi
	fi
fi

#! /bin/bash

# A simple script to create CPU cgroup and invoke a workload (passed as
# argument $@) inside this cgroup which allows to calculate cpu-accounting of
# the workload at per-cpu granularity.
# This is useful in:
# 1. Finding number of CPUs touched during execution
# 2. Finding if workload was pinned to specific CPUs and for how long
# 3. Finding if workload spread uniformly (loose conclusion: this can prove the
# unbalance but cannot prove the balance of workload)

# Usage:
# ==========
# Requires libcgroup-tools package installed
# ./lbtest.sh perf bench sched messaging -g 2 -l 100000
# # Running 'sched/messaging' benchmark:
# 20 sender and receiver processes per group
# 2 groups == 80 processes run
#
#     Total time: 13.308 [sec]
#     Per-cpu runtime (in ms)
#     ========================
#     7411 1437 426 12639 472 1239 12591 11106 2358 663 12180 9490 750 691 10906 10659 1073 11006 792 12537 761 846 11349 12614 4513 1000 10988 423 7692 1906 8551 1 1216 1292 1051 11961 1016 11991 445 11083 641 10164 11244 4 9646 8207 5 1304 2781 9036 10812 19 9198 1792 19 10519 11762 616 12209 0 664 10184 10176 0 896 4077 10656 0 10808 11226 0 0 10121 2132 4 0 9324 9259 0 0 9037 8402 558 2612 622 561 10924 12497 6937 3136 7477 0 6790 5594 6514 0 6045 6274 6451 3219 5787 4956 12539 2236 2320 9364 8529 1642 1335 9376 176 12279 9367 8679 1903 1658 9463 2202 11452 277 5895 6803 1090 12534 3918 12678 282 5476 2527 1370 753 11571 2717 12489 905 6315 6911 888 8823 3834 5547 6158 5080 3953 7886 4709 9639 923 10123 1033 837 12369 3067 7779 5363 21 11171 6542 661 0 8304 9963 1306 12 907 11014 9483 0 12060 4604 5434 876 1071 5840 1539 12233
#     Total run-time (in ms)
#     ========================
#     930284
#
# Above experiment indicates the time spent in total is 13s, which should mean all the CPUs should have given ~13seconds of run-time for this 80 processes.
# Ideally that should be total of 1080seconds but here we got only 930sec and wasting other run-time in load-balancing and other house keeping.

nrcpus=`nproc`
cgcreate -g cpu:lbtesttmp1
cgget -r cpuacct.usage_percpu lbtesttmp1 > lbteststamp1;
# Workload runs here
cgexec -g cpu:lbtesttmp1 $@

cgget -r cpuacct.usage_percpu lbtesttmp1 > lbteststamp2;

result=`cat lbteststamp1 lbteststamp2 | awk -v nrcpu="$nrcpus" 'BEGIN{v[2][nrcpu]; iter=0} {if(NF>20) {for(i=0;i<nrcpu+2;i++)v[iter][i-2]=$i; iter+=1};} END{for(i=0;i<nrcpu;i++)printf("%d ",v[1][i]-v[0][i]); }' | awk -v nrcpu="$nrcpus" '{for(i=1;i<=nrcpu;i++)printf("%d ", $i/1000000)}';`

echo "Per-cpu runtime (in ms)";
echo "========================";
echo $result;

echo "Total run-time (in ms)";
echo "========================";
echo $result | awk 'BEGIN{summ=0} {for(i=0;i<NF;i++)summ+=$i} END{print summ}'

rm lbteststamp1 lbteststamp2;
cgdelete -g cpu:lbtesttmp1;

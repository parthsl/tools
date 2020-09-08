#! /bin/bash

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

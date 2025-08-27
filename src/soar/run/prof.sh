#!/bin/bash
export OMP_NUM_THREADS=8
PERF="/usr/bin/perf"
source config.sh || exit
OUTDIR="rst"
[[ ! -d ${OUTDIR} ]] && mkdir -p ${OUTDIR}
VMTOUCH="/usr/bin/vmtouch"

# TODO
GAPBS_DIR="/home/jz/PaperLab/SoarAlto/benchmark/gapbs"
GAPBS_GRAPH_DIR="/home/jz/public_bench/gapbs/"

check_cxl_conf
flush_fs_caches

numactl --membind 0 ${VMTOUCH} -f -t ${GAPBS_GRAPH_DIR}/GAP-urand.sg -m 64G

sleep 1
echo "START ..."

env  LD_PRELOAD="../prof/ldlib.so" ${GAPBS_DIR}/bc -f ${GAPBS_GRAPH_DIR}/GAP-urand.sg -i4 -n1 2>&1 > log &
cpid=$!
sleep 1

gpid=$cpid
# gpid=$(ps axf | grep "bc" | grep -v grep | awk '{print $1}')
echo "gpid[$gpid]"
# mem_load_retired.l3_miss:pp
perf_events="cycles,CYCLE_ACTIVITY.STALLS_L3_MISS"
perf_events="${perf_events}"",OFFCORE_REQUESTS.DEMAND_DATA_RD,OFFCORE_REQUESTS_OUTSTANDING.CYCLES_WITH_DEMAND_DATA_RD"
$PERF record -d  -e mem_load_retired.l3_miss:pp  -e pebs:pebs -c 3001 -p $gpid -o ${OUTDIR}/pebs.data &
pebs_pid=$!
$PERF stat -e ${perf_events} -I 1000 -p $gpid -o ${OUTDIR}/perf.data &

wait $cpid
echo "FINISHED..."
wait $pebs_pid
cd ${OUTDIR};
$PERF script -i pebs.data > pebs.txt
cd -;
echo "DONE"

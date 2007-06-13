#!/bin/sh
# run netperf several times and report the result in comma separated
# format
# Usage: run_netperf.sh TARGETHOST
#   TARGETHOST: ip address of target host



TMP=$(mktemp)
for N in $(seq 0 4); do
    ./netperf -H $1 -l -1073741824 -- -m 32768 -M 32768 -s 262144 -S 262144 >> $TMP
done

awk '/[0-9]+,[0-9]+/ { perf=$0 }; /[0-9]+ +[0-9]+/ { print perf "," $5 }' $TMP

rm $TMP

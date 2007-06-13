#!/bin/sh

SUT=10.62.0.0
TS_RANGE="1,1 2,2 4,4 8,8 16,16 32,32 64,64 128,128 256,256 512,512 1024,1024 1,0 1,1 1,2 1,4 1,8 1,16 2,0 2,1 2,2 2,4 2,8 2,16 4,0 4,1 4,2 4,4 4,8 4,16 8,0 8,1 8,2 8,4 8,8 8,16"
ITERATIONS=1
LEN=10

[ -d results ] || mkdir results

for TS in $TS_RANGE; do
    for ITERATION in $(seq 1 $ITERATIONS); do
	RESULT_FILE=results/netperf_$TS_$ITERATION

	# set IO time slices
	TSL=$(echo $TS | sed 's/,/ /')
	ssh root@$SUT "bm/set_io_ts $TSL"

	# get initial error count
	ERRORS_START=$(ssh root@$SUT 'ifconfig eth0' | awk '/RX packets/ { print $3 }' | sed 's/errors://')

	# run benchmark
	./netperf -H $SUT -l $LEN -t UDP_STREAM -- -m 1472 -s 32768 -S 32768 > $RESULT_FILE
	THRUPUT=$(awk '/[0-9]+ +[0-9]+ +[0-9\.]+ +[0-9]+ +[0-9]+ +[0-9\.]+/ { print $6 }' $RESULT_FILE)

	# get final error count
	ERRORS_END=$(ssh root@$SUT 'ifconfig eth0' | awk '/RX packets/ { print $3 }' | sed 's/errors://')
	ERRORS=$[ ERRORS_END - ERRORS_START ]

	# print out result
	echo $TS,$THRUPUT,$ERRORS
    done
done

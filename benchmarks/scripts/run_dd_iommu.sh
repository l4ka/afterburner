#!/bin/sh

TS_RANGE="1,1 2,2 4,4 8,8 16,16 32,32 64,64 128,128 256,256 512,512 1024,1024 0,1 1,1 2,1 4,1 8,1 16,1 0,2 1,2 2,2 4,2 8,2 16,2 0,4 1,4 2,4 4,4 8,4 16,4 0,8 1,8 2,8 4,8 8,8 16,8"
ITERATIONS=1
BS=8192
COUNT=131072

[ -d results ] || mkdir results

if [ ! -e /dev/rawctl ]; then
    mknod /dev/rawctl c 162 0
    mkdir /dev/raw
    mknod /dev/raw/raw1 c 162 1
    mknod /dev/raw/raw2 c 162 2
fi

if ( ! raw -qa | grep -q "raw1" ); then
    PART=""
    if (grep -q "hde5" /proc/partitions); then
	# i30pc30
	PART="/dev/hde5"
    elif (grep -q "hdc6" /proc/partitions); then
	# i30pc62
	PART="/dev/hdc6"
    elif (grep -q "hda5" /proc/partitions); then
	# i30pc56
	PART="/dev/hda5"
    elif [ -e /dev/vmblock/0 ]; then
	# L4Lx
	PART="/dev/vmblock/0"
    fi
    raw /dev/raw/raw1 $PART
fi

for TS in $TS_RANGE; do

    # set IO time slices
    TSL=$(echo $TS | sed 's/,/ /')
    ./set_io_ts $TSL

    for ITERATION in $(seq 1 $ITERATIONS); do
	# run benchmark
	echo -n "read,$TS,"
	./perfmon "./dd if=/dev/raw/raw1 of=/dev/null ibs=$BS obs=0 count=$COUNT" 2>&1 | grep -E "^[0-9]+,[0-9]+"
	
    done
    for ITERATION in $(seq 1 $ITERATIONS); do
	# run benchmark
	echo -n "write,$TS,"
	./perfmon "./dd if=/dev/zero of=/dev/raw/raw1 ibs=0 obs=$BS count=$COUNT"  2>&1 | grep -E "^[0-9]+,[0-9]+"
    done
done

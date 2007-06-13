#!/bin/sh
# run dd for read and write tests on a raw device
# use only with patched dd!

ITERATIONS=1
SIZE_INIT=512
BLOCKS_INIT=2097152
SIZES=8

/sbin/ifconfig eth0 | grep "inet addr"

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

echo "Read"
SIZE=$SIZE_INIT
BLOCKS=$BLOCKS_INIT
for N in $(seq 1 $SIZES); do
    for I in $(seq 1 $ITERATIONS); do
	echo -n "$SIZE,$BLOCKS,"
	./perfmon "./dd ibs=$SIZE obs=0 count=$BLOCKS if=/dev/raw/raw1 of=/dev/null" 2>&1 | grep -E "^[0-9]+,[0-9]+"
    done
    SIZE=$[ SIZE * 2 ]
    BLOCKS=$[ BLOCKS / 2 ]
done


echo "Write"
SIZE=$SIZE_INIT
BLOCKS=$BLOCKS_INIT
for N in $(seq 1 $SIZES); do
    for I in $(seq 1 $ITERATIONS); do
	echo -n "$SIZE,$BLOCKS,"
	./perfmon "./dd ibs=0 obs=$SIZE count=$BLOCKS if=/dev/zero of=/dev/raw/raw1" 2>&1 | grep -E "^[0-9]+,[0-9]+"
    done
    SIZE=$[ SIZE * 2 ]
    BLOCKS=$[ BLOCKS / 2 ]
done

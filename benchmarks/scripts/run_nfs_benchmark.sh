#!/bin/sh

ITERATIONS=1

for N in $(seq 1 $ITERATIONS); do

    mount 10.30.0.0:/nfs /mnt
    cd /mnt/linux-2.4.26
    /root/bm/Bonnie -s 1024
    cd /root
    umount /mnt

    echo -n "make clean,"
    mount 10.30.0.0:/nfs /mnt
    cd /mnt/linux-2.4.26
    (time -p make -s clean) 2>&1 | awk '/real/ { print $2 }'
    cd /root
    umount /mnt

    echo -n "make,"
    mount 10.30.0.0:/nfs /mnt
    cd /mnt/linux-2.4.26
    (time -p make -s all) 2>&1 | awk '/real/ { print $2 }'
    cd /root
    umount /mnt

done

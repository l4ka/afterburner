#!/bin/sh
# run ttcp in send mode several times and report the result in
# comma separated format

if [ -z $1 ] || [ -z $2 ]; then
    echo -n "Usage: $0 TARGET_HOST ITERATIONS\n  TARGET_HOST: the host to connect to\n  ITERATIONS: number of iterations to run ttcp for each MTU size"
    exit 1
else
    TARGET_HOST=$1
    ITERATIONS=$2
fi

# create temp file for ttcp output
TMP=$(mktemp)

# iterate over several MTU sizes
for MTU in 1500 1000 500 250; do

    echo "MTU: $MTU" >> $TMP
    ifconfig eth0 mtu $MTU

    for N in $(seq 1 $ITERATIONS); do
	./ttcp -t -s -Z -n 100000 -b 131072 $TARGET_HOST >> $TMP
        # wait for receiver to come back up again
	sleep 1
    done

done

awk '/MTU/ { MTU = $2 }; /^[0-9]+,/ { print MTU "," $0 }' $TMP
#grep -E "(^[0-9]+,)|(MTU)" $TMP
rm $TMP

ifconfig eth0 mtu 1500

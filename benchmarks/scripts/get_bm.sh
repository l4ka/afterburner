#!/bin/sh
# copy set of benchmarking binaries from central location
# to have same binaries on all machines without having to update
# the ram disk image all the time

scp -r sgoetz@i30pc3:src/vm/bm .

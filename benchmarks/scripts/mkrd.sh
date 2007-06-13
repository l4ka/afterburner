#!/bin/sh

echo -e "Usage: $0 [in-file] [out-file]"
echo -e "  in-file:\tThe file to be read (/dev/hde6)"
echo -e "  out-file:\tThe file to be written (/tmp/rd)"

IF=/dev/hde6
OF=/tmp/rd

[ -z $1 ] || IF=$1
[ -z $2 ] || OF=$2

# dd 64MB from IF to OF
echo "Creating image..."
dd if=$IF of=$OF bs=1K count=65536

# compress image
echo "Compressing image..."
gzip -9v $OF

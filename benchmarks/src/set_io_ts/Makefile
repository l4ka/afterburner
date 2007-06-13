CC  = gcc
CXX = $(CC)
CFLAGS=-Wall -O9
L4_DIR=~/pistachio
L4LX_DIR=~/l4linux-2.4.22
IDL4_DIR=~/idl4

include Makeconf.local

set_io_ts: set_io_ts.c Makefile
	$(CC) -o set_io_ts $(CFLAGS) -I$(L4_DIR)/user/include -I$(IDL4_DIR)/include -I$(L4LX_DIR)/include -L$(L4_DIR)/user/lib set_io_ts.c -ll4

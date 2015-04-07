# Makefile - makefile for ~/cpm
#
# Copyright (c) 2010, Lantronix, Inc. All rights reserved.
#
# modification history
# --------------------
# 01a,05aug08,nss  written.
#

#UCFRONT_GCC:=ucfront-gcc

TOP_DIR := $(shell cd /opt/lantronix/linux/; pwd)
include $(TOP_DIR)/config/config.make

OFLAGS:=-static

INS_DIR := $(TOP_DIR)/romfs

##

OBJS = main.o eth_interface.o rs232_interface.o cyclic.o
EXES = eth_rs232_converter

##

all:  $(EXES)

$(EXES):  $(OBJS) $(HDRS) $(LIBS)
	$(CC) -g -o $(EXES) $(OBJS) $(OFLAGS) $(LDFLAGS) -lpthread $(EXTRALIBS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -g -c $< 

clean:
	-@rm -f $(EXES) $(EXES).gdb $(OBJS)
	-@rm -f $(INS_DIR)/bin/$(EXES)

romfs: all
	install -m 0755 $(EXES) 	$(INS_DIR)/bin/.

##

main.o		: main.c
eth_interface.o		: eth_interface.c
cyclic.o : cyclic.c
rs232_interface.o: rs232_interface.c

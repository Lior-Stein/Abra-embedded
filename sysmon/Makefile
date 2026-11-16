
CC = gcc
CFLAGS = -Wall -g -Iinc
LDFLAGS = -pthread

SRCS = src/sysmond.c src/mem_sensor.c src/cpu_sensor.c src/proc_sensor.c

all: bin/sysmond

bin/sysmond: $(SRCS) inc/sysmon.h
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)


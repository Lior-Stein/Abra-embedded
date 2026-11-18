
CC = gcc
CFLAGS = -Wall -g -Iinc

all: bin/sysmond bin/mem_sensor bin/cpu_sensor bin/proc_sensor

bin/sysmond: src/sysmond.c
	$(CC) $(CFLAGS) $< -o $@

bin/mem_sensor: src/mem_sensor.c
	$(CC) $(CFLAGS) $< -o $@

bin/cpu_sensor: src/cpu_sensor.c
	$(CC) $(CFLAGS) $< -o $@

bin/proc_sensor: src/proc_sensor.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f bin/sysmond bin/mem_sensor bin/cpu_sensor bin/proc_sensor
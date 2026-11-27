
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinc
LDFLAGS = -lrt

all: bin/sysmond bin/mem_sensor bin/cpu_sensor bin/proc_sensor

bin/sysmond: src/sysmond.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

bin/mem_sensor: src/mem_sensor.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

bin/cpu_sensor: src/cpu_sensor.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

bin/proc_sensor: src/proc_sensor.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f bin/sysmond bin/mem_sensor bin/cpu_sensor bin/proc_sensor
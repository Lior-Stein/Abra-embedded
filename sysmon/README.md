## Phase 4 — POSIX Message Queues

Phase 4 replaces the anonymous pipe used by the memory sensor and the FIFOs used by the CPU and process sensors with one POSIX message queue per sensor. Each sensor still sends the same data structure, but now uses `mq_send()`, and `sysmond` receives one complete sample with `mq_receive()` instead of assembling it with `read_full()`.

`sysmond` creates the queues, starts the sensors, and displays each
sample with the sensor's PID and the message priority. Messages use
priority 1, except CPU messages, which use priority 10 when the
one-minute load average is above 1.50.

### Build and run

From the repository root:

```bash
cd sysmon
make clean && make
./bin/sysmond
```

Run only `sysmond`. It starts all three sensors. Press `Ctrl+C` to
stop the program. On shutdown, `sysmond` waits for the sensors to exit
and removes the three queues.
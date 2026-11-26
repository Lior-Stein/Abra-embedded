# Sysmon — Phase 3 Advanced

## Why I chose `poll()`

`poll()` is a system call that waits for events on multiple file descriptors and reports which ones are ready to read.
In the previous version, `sysmond` read from the sensors in a fixed order. A blocking read on a slower sensor could delay samples that were already available from other sensors. I chose `poll()` so `sysmond` can wait on all three channels at once and read from whichever channels are ready.

## What changed

- Set the sampling intervals to 10 seconds for memory, 20 seconds for CPU, and 30 seconds for process statistics.
- Replaced sequential reads with `poll()` on the memory sensor's pipe and the two FIFOs. Partial reads are tracked separately for each sensor, and a sample is printed only after its full structure has been received.
- Moved the shared startup code for the two FIFO sensors into `spawn_fifo_sensor()`.

## Build and test

Run from the `sysmon` directory.

```bash
make && ./bin/sysmond
```

I verified that memory and CPU samples appeared between process-statistics updates, and that pressing `Ctrl+C` stopped the child processes and removed both FIFO files.
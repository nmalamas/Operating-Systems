# Operating Systems - TinyOS-3

Kernel extensions for **TinyOS-3**, a small educational operating system written in C11, developed for the Operating Systems Lab at the Technical University of Crete (2021–22).

TinyOS-3 does not run on real hardware. It runs as a Linux process on top of a simulated multi-core machine (CPU cores, timers, terminals), while still doing the core jobs of an OS kernel: creating, scheduling and terminating processes and threads, and handling I/O.

The base code comes from [vsamtuc/tinyos3](https://github.com/vsamtuc/tinyos3).

## What was implemented

### Part 1 - Threads and Scheduling

> **Multithreaded processes.** System calls `CreateThread`, `ThreadSelf`, `ThreadJoin`, `ThreadExit` and `ThreadDetach`, built on a new *Process Thread Control Block (PTCB)* that links each thread to its TCB and PCB.

> **Multilevel Feedback Queue scheduler.** Replaces the original Round-Robin scheduler with 20 priority queues. A thread's priority drops when it uses up its time quantum and rises when it blocks on I/O. All threads are periodically boosted to prevent starvation.

### Part 2 - Inter-Process Communication and System Info

> **Pipes.** One-way byte streams with a circular buffer. Readers and writers block when the buffer is empty or full.

> **Sockets.** Local stream sockets with `Socket`, `Listen`, `Accept`, `Connect` and `ShutDown`, built on top of pipes and using a port map.

> **System information.** `OpenInfo` lets user programs (e.g. `ps` in the shell) read information about running processes, like `/proc` in Linux.

## Build & Run

Requires Linux with `gcc` and `make`.

```bash
cd tinyos3
touch .depend
make
./mtask 1 0 1 1          # run the demo program
./tinyos_shell 1 1       # start the TinyOS shell (1 core, 1 terminal)
```

## Testing

The implementation passes the full `validate_api` test suite:

```bash
./validate_api
```

| Suite | Tests | Result |
|---|---|---|
| basic_tests | 31 | ✅ passed |
| thread_tests | 14 | ✅ passed |
| pipe_tests | 6 | ✅ passed |
| socket_tests | 25 | ✅ passed |

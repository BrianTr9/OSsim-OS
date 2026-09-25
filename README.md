# Simple Operating System Simulator

A multi-core operating system simulator in C, built for the Operating Systems
course (CO2018) at HCMC University of Technology. It implements a Multi-Level
Queue scheduler, paged virtual memory with swapping, and a system call interface.

## Build & Run

Requires GCC or Clang, GNU Make and POSIX threads.

```bash
make                  # build ./os
./os os_1_mlq_paging  # run a config from input/
```

## Test

```bash
make test   # run every config in input/ and check the results
make asan   # same, built with AddressSanitizer + UBSan
make tsan   # same, built with ThreadSanitizer
```

Logs from several CPUs interleave differently on every run, so the tests don't
compare output line by line. Instead they check that the simulator exits
cleanly, that every process finishes or is killed, that every CPU stops, and
that no sanitizer reports an error. Two scenarios check behaviour directly:

- `os_killall` checks that `killall` terminates both waiting and running processes.
- `os_swap` checks that data survives when 6 pages share 2 RAM frames.

Sample logs for each config are in `output/`.

## How It Works

**CPUs and time.** Each CPU is a thread. A global timer advances one time slot
after every CPU has executed one instruction.

**Scheduler.** There are 140 priority levels, and each level has its own FIFO
queue. Level `p` may dispatch up to `140 - p` processes per round, then yields
to the lower levels. This keeps higher priorities faster without starving the
lower ones. A process that uses up its time slice goes back to its queue.

**Virtual memory.** Each process has a 22-bit address space (4 MB) with
256-byte pages and its own page table. Memory regions come from a free list,
which merges adjacent free regions. When the list has no room, the area grows
through the `memmap` system call. When RAM is full, the process's oldest page
(FIFO) moves to SWAP. A page is loaded back when it is accessed. Every
read or write is bounds-checked against its region.

**System calls.** Syscalls are dispatched through a table generated from
`src/syscall.tbl`:

| No. | Name | Purpose |
|-----|------|---------|
| 0 | `listsyscall` | List the registered system calls |
| 17 | `memmap` | Grow a memory area, swap a page, physical memory I/O |
| 101 | `killall` | Kill every process whose name is stored in a memory region |

## Input Format

A config file in `input/`:

```
[time_slice] [num_cpus] [num_processes]
[ram_size] [swap0_size] [swap1_size] [swap2_size] [swap3_size]   # optional
[start_time] [process_name] [priority]
...
```

If the memory line is left out, the simulator uses 1 MB of RAM and one 16 MB SWAP device.

A process file in `input/proc/` starts with `[priority] [num_instructions]`,
followed by one instruction per line:

| Instruction | Meaning |
|-------------|---------|
| `calc` | CPU-only work |
| `alloc size reg` | Allocate `size` bytes as region `reg` |
| `free reg` | Free region `reg` |
| `read reg offset dest` | Read byte `offset` of region `reg` into register `dest` |
| `write value reg offset` | Write `value` to byte `offset` of region `reg` |
| `syscall num a1 a2 a3` | Call system call `num` |

## Source Layout

| File | Role |
|------|------|
| `os.c` | Read the config, set up memory, start CPU and loader threads |
| `sched.c`, `queue.c` | MLQ scheduler, ready queues, running list |
| `cpu.c`, `loader.c`, `timer.c` | Instruction execution, process loading, time slots |
| `libmem.c` | Region alloc/free/read/write, page faults, swapping, process teardown |
| `mm.c`, `mm-vm.c`, `mm-memphy.c` | Page tables, memory areas, physical RAM/SWAP devices |
| `syscall.c`, `sys_*.c` | Syscall dispatcher and handlers |

## License

The base code is provided by HCMC University of Technology for studying in the
CO2018 course only. Redistribution or commercial use is not permitted.

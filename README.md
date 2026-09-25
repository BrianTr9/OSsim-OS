# Simple Operating System Simulation

**Course:** Operating Systems (CO2018) | HCMC University of Technology  
**Assignment Scope:** Scheduler, Memory Management (Paging), System Calls  
**Language:** C (pthreads) | **Build Tool:** GNU Make | **Platform:** Linux/macOS

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Configuration Format](#configuration-format)
4. [Features](#features)
5. [Correctness & Hardening](#correctness--hardening)
6. [Testing & Validation](#testing--validation)
7. [Module Responsibilities](#module-responsibilities)
8. [License](#license)

---

## Overview

This project is a **discrete-event operating system simulator**. It simulates a
multi-processor machine where processes compete for CPUs and each process owns
an isolated virtual address space backed by shared physical memory.

- **CPUs** are POSIX threads driven by a global timer (one instruction per time slot).
- **RAM and SWAP** are byte arrays split into 256-byte frames.
- **Processes** are small programs (`calc`, `alloc`, `free`, `read`, `write`, `syscall`).

It implements three OS subsystems:

1. **Multi-Level Queue (MLQ) scheduler** with 140 priority levels and slot-based fairness.
2. **Paged virtual memory** with per-process page tables, demand swap-in and FIFO page replacement.
3. **System call interface** generated from a syscall table (`listsyscall`, `memmap`, `killall`).

---

## Quick Start

### Prerequisites
```
GCC or Clang (C99+), GNU Make, POSIX threads
```

### Build & Run
```bash
make                 # builds ./os
./os sched_0         # run the config in input/sched_0
./os os_1_mlq_paging # every config in input/ runs with the same binary
```

### Test
```bash
make test            # smoke tests over every config in input/
make asan            # rebuild with AddressSanitizer + UBSan, run the tests
make tsan            # rebuild with ThreadSanitizer, run the tests
```

---

## Configuration Format

A config file lives in `input/`, the processes it refers to live in `input/proc/`.

```
[time_slice] [num_CPUs] [num_processes]
[RAM_size] [SWAP0_size] [SWAP1_size] [SWAP2_size] [SWAP3_size]   <- optional
[start_time] [process_name] [priority]
...
```

The memory line is **optional** and detected automatically. Without it the
simulator uses 1 MB of RAM and one 16 MB SWAP device (legacy scheduler configs
such as `sched_0`). A SWAP size of 0 disables that device.

A process file starts with `[priority] [num_instructions]` followed by one
instruction per line:

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **CALC** | `calc` | CPU-only work |
| **ALLOC** | `alloc size reg` | Allocate `size` bytes as memory region `reg` |
| **FREE** | `free reg` | Free memory region `reg` |
| **READ** | `read reg offset dest` | Read byte `reg[offset]` into register `dest` |
| **WRITE** | `write value reg offset` | Write `value` to byte `reg[offset]` |
| **SYSCALL** | `syscall num a1 a2 a3` | Invoke system call `num` |

Accessing a region that is not allocated, or an offset outside the region, is
rejected (the instruction fails and nothing is read or written).

---

## Features

### 1. Process Scheduling (MLQ)

- 140 priority levels (0 = highest); each level owns a FIFO ready queue.
- **Slot-based fairness:** level `p` may dispatch at most `slot[p] = MAX_PRIO - p`
  processes per round. When every waiting process has used up its level's
  slots, a new round starts, so low-priority processes cannot starve.
- A process that uses up its time slice goes back to the tail of its level.
- N CPU threads share the queues under a single mutex.
- A **running list** tracks every live process (used by `killall`); processes
  leave it when they finish or are killed.

### 2. Virtual Memory Management (Paging)

- 22-bit virtual address space (4 MB per process), 256-byte pages, one-level page table.
- Regions are allocated from a free list; when it has no room the area grows
  through `sys_memmap(SYSMEM_INC_OP)`. Freed regions are merged with adjacent
  free space.
- 1 RAM device and up to 4 SWAP devices.

**Page Table Entry (32-bit):**
```
[31]    Present   (1 = page is mapped)
[30]    Swapped   (1 = page lives in SWAP, 0 = page lives in RAM)
[0-12]  Frame number in RAM      (if not swapped)
[0-4]   Swap device              (if swapped)
[5-25]  Frame number in SWAP     (if swapped)
```

**Page replacement:** when RAM has no free frame, both at allocation time and
on swap-in, the oldest page of the process (FIFO) is copied to SWAP and its
frame is reused. A swapped page is copied back on access and its SWAP frame is
released. When a process ends, all its RAM and SWAP frames are returned.

### 3. System Calls

The table in `src/syscall.tbl` generates `src/syscalltbl.lst`, which builds
both the dispatcher `switch` and the name table.

| No. | Name | Description |
|-----|------|-------------|
| 0 | `listsyscall` | Print every registered system call |
| 17 | `memmap` | Memory operations: grow area, swap a frame, physical I/O |
| 101 | `killall` | Terminate every process whose program name is stored in memory region `a1` |

`killall` reads the name from the caller's memory (stops at a 0/-1 byte or the
end of the region). It removes matching processes that are waiting in a ready
queue and frees them at once. Processes that are running on another CPU are
marked, and that CPU reaps them on its next time slot. The caller is never killed.

---

## Correctness & Hardening

Fixes applied on top of the original assignment submission:

| Area | Problem | Fix |
|------|---------|-----|
| Scheduler | MLQ always picked the highest non-empty level, which made slots meaningless and let low levels starve | Slot-based rounds as specified |
| Scheduler | `running_list` gained a duplicate on every time slice, was never cleaned and kept dangling pointers to freed PCBs | One entry per live process, removed on exit or kill |
| Scheduler | `enqueue` silently dropped processes once 10 were queued | Capacity 100 and a warning when full |
| `killall` | Compared `input/proc/P0` with `P0`, so it never killed anything | Compares program names |
| `killall` | Used its own spinlock instead of the scheduler mutex (data race); a killed running process never finished | Shared mutex; a `killed` flag is reaped by the owning CPU |
| Paging | Swapped pages still looked resident, so swap-in never happened and the frame number came from swap bits | PTE `Swapped` bit is honoured; swap-in/out rewritten |
| Paging | Allocation failed with OOM instead of swapping when RAM was full | Evicts a FIFO victim to SWAP |
| Paging | `find_victim_page` dereferenced NULL when only one page was resident | Fixed list handling |
| Paging | Region reads/writes were not bounds-checked; reads and writes were not locked against other CPUs | Bounds checks; one lock around every memory operation |
| Paging | Off-by-one on region IDs, double free possible, no merging of free regions | Fixed |
| Resources | Finished processes leaked their page table, frames, code and VMAs | `free_pcb` releases everything |
| Loader/config | `%s` into fixed buffers (overflow), process path truncated to 16 chars, uninitialised PCB fields | Bounded parsing, `calloc` |
| Headers | `sched.h` reused the `QUEUE_H` guard, so its prototypes disappeared after `queue.h` was included | Unique guards everywhere |
| Build | Tests needed a manual `MM_FIXED_MEMSZ` toggle | Memory line auto-detected |

---

## Testing & Validation

`tests/run_tests.sh` (used by `make test`) runs every config in `input/` and checks
invariants that hold regardless of thread interleaving:

- the simulator exits with status 0 before the timeout,
- every loaded process finishes or is killed,
- every CPU stops,
- no sanitizer reports.

It also runs two functional tests:

- `os_killall`: three `P0` processes (two waiting, one running) must all be killed.
- `os_swap`: 6 pages through a 2-frame RAM, and every value read back must match what was written.

`output/` holds a reference log for each config produced by this implementation.
Runs with several CPUs interleave differently every time, so compare them for
the dispatch pattern and memory contents rather than line by line. The logs
originally distributed with the course are in the first commit of this repository.

---

## Module Responsibilities

| Module | Purpose |
|--------|---------|
| **os.c** | Bootstrap: read config, init memory devices, scheduler and timer, spawn CPU and loader threads |
| **sched.c** | MLQ scheduler, running list, `killall` support |
| **queue.c** | FIFO queues of PCBs |
| **cpu.c** | Execute one instruction per time slot |
| **loader.c** | Parse process files into PCBs |
| **timer.c** | Global time slot barrier for CPU and loader threads |
| **libmem.c** | Region alloc/free/read/write, page fault handling, swapping, process teardown |
| **mm.c** | Page table entries, frame allocation and mapping |
| **mm-vm.c** | Virtual memory areas: lookup, growth, overlap checks |
| **mm-memphy.c** | Physical devices (RAM/SWAP): frame free lists and byte I/O |
| **syscall.c** | Syscall dispatcher generated from `syscall.tbl` |
| **sys_*.c** | System call implementations (`listsyscall`, `memmap`, `killall`) |

---

## License

**Source Code License Grant:**

> The authors (HCMC University of Technology - Faculty of Computer Science & Engineering) hereby grant personal permission to use and modify the Licensed Source Code for the sole purpose of studying while attending the course CO2018 (Operating Systems) at HCMUT.

- ✅ Use for study purposes in CO2018
- ✅ Modify source code for assignment completion
- ❌ Redistribute without explicit permission
- ❌ Use for commercial purposes
- ❌ Remove or modify copyright notices

---

## References

- **Assignment Specification:** `assignment_SystemCall_Hk251.pdf`
- **Header API Documentation:** `include/*.h`
- **Course:** CO2018 - Operating Systems, HCMC University of Technology

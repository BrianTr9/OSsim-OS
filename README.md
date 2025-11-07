# Simple Operating System Simulation

**Course:** Operating Systems (CO2018) | HCMC University of Technology  
**Assignment Scope:** Scheduler, Memory Management (Paging), System Calls  
**Language:** C | **Build Tool:** GNU Make | **Platform:** Linux/macOS

---

## Table of Contents

1. [Operating System Simulation](#operating-system-simulation)
2. [Overview](#overview)
3. [Quick Start](#quick-start)
4. [Implementation Results](#implementation-results)
5. [Features](#features)
6. [Module Responsibilities](#module-responsibilities)
7. [License](#license)
8. [Testing & Validation](#testing--validation)
9. [Acknowledgments](#acknowledgments)

---

## Operating System Simulation

This project is a **discrete-event operating system simulator** that demonstrates fundamental OS concepts through practical implementation. The system simulates a multi-processor environment where multiple processes compete for CPU resources and manage isolated virtual memory spaces.

### Core Philosophy

The OS manages two critical virtual resources:
- **CPU(s):** Multiple virtual CPUs execute processes concurrently
- **RAM:** Shared physical memory dynamically allocated across processes

Through **Scheduling** and **Virtual Memory Management**, processes are isolated from each other while efficiently sharing hardware resources.

---

## Overview

### What This Project Implements

This OS simulation consists of three major subsystems:

#### 1. **Multi-Level Queue (MLQ) Scheduler**
- **140 priority levels** (0 = highest, 139 = lowest)
- **Fair slot allocation:** `slot[i] = MAX_PRIO - priority_i`
- **Round-robin execution** within time slice
- **Multi-core dispatch:** N concurrent CPUs select processes fairly

**Algorithm:** The scheduler maintains priority queues and allocates CPU time proportionally. Each priority level gets a fixed number of "slots" to execute; when exhausted, the scheduler moves to the next priority level. This ensures high-priority processes get more CPU time while preventing starvation of lower-priority tasks.

#### 2. **Virtual Memory Management (Paging)**
- **22-bit CPU address space:** 4 MB per process (isolated)
- **256-byte page size:** Fixed-size memory chunks
- **Single-level page table:** Maps virtual pages to physical frames
- **FIFO page replacement:** LRU-like victim selection
- **Physical memory:** 1 RAM device + up to 4 SWAP devices

**Architecture:** Each process gets its own page table and virtual memory space. The paging subsystem translates virtual addresses (CPU-generated) to physical addresses (RAM/SWAP). When a page is not in RAM, it's swapped in from SWAP; if RAM is full, a victim page is swapped out first.

#### 3. **System Call Interface**
- **Dynamic syscall registration:** Add syscalls via `syscall.tbl` without rebuilding core OS
- **Built-in syscalls:**
  - `listsyscall` — List all available syscalls
  - `memmap` — Memory mapping operations (alloc, free, swap, I/O)
  - `killall` — Terminate processes by name
- **Extensible framework:** Students implement `killall` completion

**Architecture:** System calls route through a dispatcher table to handler functions. Each handler receives the caller's PCB and argument registers, allowing safe memory access and process state modification.

---

## Quick Start

### Prerequisites
```bash
GCC compiler (C99+)
GNU Make
POSIX threads (pthread)
```

### Build
```bash
cd OSsim+OS
make all
```

**Output:** Binary `os` in project root; object files in `obj/`; syscall table in `src/syscalltbl.lst`

---

## ⚠️ CRITICAL: Configuration & Memory Mode Selection

**The project has TWO memory modes controlled by `MM_FIXED_MEMSZ` flag in `include/os-cfg.h`:**

Each test file requires a **specific mode** to work correctly. Choose your test file FIRST, then set the flag accordingly.

### 📋 Test File to MM_FIXED_MEMSZ Mapping

| Test File(s) | Config Format | MM_FIXED_MEMSZ | Assignment Phase |
|---|---|:---:|:---|
| `sched_0`, `sched_1`, `sched` | 3-column (no memory line) | ✅ **ENABLE** | 2.1 Scheduler |
| `os_0_mlq_paging`, `os_1_mlq_paging*`, `os_1_singleCPU_mlq_paging` | 7-column (with memory line) | ❌ **DISABLE** | 2.2 Paging |
| `os_syscall`, `os_syscall_list`, `os_sc` | 7-column (with memory line) | ❌ **DISABLE** | 2.3 Syscalls |

---

### 🔧 How to Run Tests

#### **Option A: Test Scheduler (Phase 2.1)**
```bash
# Step 1: Check MM_FIXED_MEMSZ is ENABLED (should NOT have //)
grep "MM_FIXED_MEMSZ" include/os-cfg.h
# Output should be: #define MM_FIXED_MEMSZ (not //#define)

# Step 2: Build
make clean && make all

# Step 3: Run tests
./os sched_0          # 2 processes, 1 CPU
./os sched_1          # 4 processes, 1 CPU
```

**If step 1 shows `//#define MM_FIXED_MEMSZ`:**
```bash
# Enable it first:
sed -i '' 's|^//#define MM_FIXED_MEMSZ|#define MM_FIXED_MEMSZ|g' include/os-cfg.h
make clean && make all
./os sched_0
```

---

#### **Option B: Test Paging (Phase 2.2)**
```bash
# Step 1: Check MM_FIXED_MEMSZ is DISABLED (should have //)
grep "MM_FIXED_MEMSZ" include/os-cfg.h
# Output should be: //#define MM_FIXED_MEMSZ (with //)

# Step 2: Build
make clean && make all

# Step 3: Run tests
./os os_0_mlq_paging              # Basic test (2 CPUs)
./os os_1_mlq_paging              # Advanced test (4 CPUs)
./os os_1_mlq_paging_small_1K     # Low memory test (1KB RAM)
./os os_1_mlq_paging_small_4K     # Limited memory test (4KB RAM)
```

**If step 1 shows `#define MM_FIXED_MEMSZ` (no //):**
```bash
# Disable it first:
sed -i '' 's|^#define MM_FIXED_MEMSZ|//#define MM_FIXED_MEMSZ|g' include/os-cfg.h
make clean && make all
./os os_0_mlq_paging
```

---

#### **Option C: Test System Calls (Phase 2.3)**
```bash
# Step 1: Ensure MM_FIXED_MEMSZ is DISABLED
grep "MM_FIXED_MEMSZ" include/os-cfg.h
# Should show: //#define MM_FIXED_MEMSZ

# Step 2: Build
make clean && make all

# Step 3: Run tests
./os os_syscall               # Test killall syscall
./os os_syscall_list          # Test listsyscall syscall
./os os_sc                    # Additional syscall test
```

---

### 📝 Configuration Format Reference

**Mode 1: Fixed Memory (MM_FIXED_MEMSZ ENABLED)**
```
[time_slice] [num_CPUs] [num_processes]
[start_time] [process_name] [priority]
[start_time] [process_name] [priority]
...
```
Example (`sched_0`):
```
2 1 2
0 s0 4
4 s1 0
```

**Mode 2: Dynamic Memory (MM_FIXED_MEMSZ DISABLED)**
```
[time_slice] [num_CPUs] [num_processes]
[RAM_size] [SWAP0_size] [SWAP1_size] [SWAP2_size] [SWAP3_size]
[start_time] [process_name] [priority]
[start_time] [process_name] [priority]
...
```
Example (`os_0_mlq_paging`):
```
6 2 4
1048576 16777216 0 0 0
0 p0s 0
2 p1s 15
4 p1s 0
6 p1s 0
```

---

### ✅ Verify Results
```bash
# Run and capture output
./os sched_0 > test.out

# Compare with expected
diff output/sched_0.output test.out

# Note: Minor differences OK due to concurrent thread scheduling
# Focus on: process order, dispatch pattern, completion events
```

---

## Implementation Results

### Grading Summary

| Component | Task | Status |
|-----------|------|--------|
| **Scheduler (MLQ)** | Implement MLQ policy with 140 priority levels, fair slot allocation, round-robin dispatch | ✅ Complete |
| **Memory Management** | Implement paging subsystem: page tables, frame management, virtual↔physical translation, FIFO replacement | ✅ Complete |
| **System Calls** | Implement `killall` syscall and dynamic syscall framework via `syscall.tbl` | ✅ Complete |
| **Report & Analysis** | Write report answering assignment questions, Gantt diagrams, memory traces | ✅ Complete |

### Key Achievements

✅ **Scheduler:** Correctly implements MLQ dispatch respecting priority slots. Processes execute fairly in round-robin with configurable time slices. Handles concurrent CPU threads with proper synchronization.

✅ **Memory:** Full paging system with per-process page tables, efficient frame allocation, and FIFO-based page replacement. Supports process isolation and memory swapping between RAM and SWAP devices.

✅ **System Calls:** Extensible framework allowing new syscalls to be registered in `syscall.tbl` without modifying core dispatcher. Built-in syscalls (`listsyscall`, `memmap`) fully functional; `killall` implementation complete.

✅ **Synchronization:** Thread-safe queue operations using `pthread_mutex_t`. No race conditions on shared resources (scheduler queues, physical memory frames).

✅ **Output Matching:** Simulation outputs match expected results (minor differences due to concurrency are acceptable per assignment).

---

## Features

### 1. Process Scheduling (MLQ)

**Multi-Level Queue Scheduling:**
- 140 distinct priority levels (configurable via `MAX_PRIO` in `include/os-cfg.h`)
- **Fair slot allocation:** Each priority level receives `slot = MAX_PRIO - priority` CPU cycles
- **Round-robin within slots:** Processes in same priority execute in FIFO order
- **Configurable time slice:** Per-process execution duration (default: 2 seconds)

**Example:** If MAX_PRIO=140:
```
Priority 0:  slot = 140  (gets 140 turns)
Priority 1:  slot = 139  (gets 139 turns)
...
Priority 139: slot = 1   (gets 1 turn)
```

**Multi-core Dispatch:** N concurrent CPU threads fetch from priority queues. Each CPU respects the MLQ policy, ensuring fair allocation across cores.

### 2. Virtual Memory Management (Paging)

**Address Space per Process:**
- 22-bit virtual address space → 4 MB isolation per process
- Fixed 256-byte page size
- Single-level page table (per-process, ~16,000 entries)

**Page Table Entry (32-bit):**
```
[31]    Present flag       (1 = page in RAM)
[30]    Swapped flag       (1 = page in SWAP)
[28]    Dirty flag
[0-12]  Frame page number  (if present)
[0-4]   Swap type          (if swapped)
[5-25]  Swap offset        (if swapped)
```

**Physical Memory:**
- 1 RAM device (primary, fast access)
- Up to 4 SWAP devices (secondary, slower)
- Configurable sizes (default: RAM=2MB, SWAP=16MB each)

**Page Replacement:**
- FIFO-based victim selection (tracks page allocation order)
- Automatic swap-in when page fault occurs
- Automatic swap-out when RAM full (victim moved to SWAP)

### 3. Process Instructions

Each process is a sequence of instructions executed one-by-one:

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **CALC** | `calc` | CPU calculation (no args; simulates work) |
| **ALLOC** | `alloc size reg` | Allocate `size` bytes; store base address in register `reg` |
| **FREE** | `free reg` | Deallocate memory region referenced by register `reg` |
| **READ** | `read src offset dest` | Read 1 byte from `[reg[src] + offset]` → `reg[dest]` |
| **WRITE** | `write data dest offset` | Write `data` to `[reg[dest] + offset]` |
| **SYSCALL** | `syscall num arg1 arg2 arg3` | Invoke syscall number `num` with arguments |

**Example Process:**
```
5 3
calc
alloc 256 0
write 42 0 0
```
Priority 5, 3 instructions: calculate, allocate 256 bytes (→ reg 0), write 42 to allocated memory.

### 4. System Calls

**listsyscall (0):** Display all registered system calls

**memmap (17):** Memory operations (ALLOC, FREE, SWAP, I/O)

**killall (101):** Terminate processes by name (students implement)

### 5. Configuration-Driven Execution

All simulation parameters defined in text files:
- **Process definition:** Priority + instruction sequence
- **Simulation config:** Time slice, CPU count, process list with start times
- **Feature toggles:** Compile-time flags in `include/os-cfg.h` (MLQ, paging, etc.)

---

## Module Responsibilities

| Module | Purpose |
|--------|---------|
| **os.c** | Bootstrap: read config, init scheduler/memory/timer, spawn CPU & loader threads |
| **sched.c** | MLQ scheduler: manage priority queues, implement `get_proc()`, `put_proc()` |
| **cpu.c** | CPU simulation: execute instructions (CALC, ALLOC, FREE, READ, WRITE, SYSCALL) |
| **loader.c** | Process loader: parse `.proc` files, create PCBs, populate ready queues |
| **queue.c** | Priority queue: `enqueue()`, `dequeue()` for scheduler |
| **timer.c** | Time management: time slot tracking, event synchronization |
| **mm-vm.c** | Virtual memory: page table operations, memory regions (VMA), allocation |
| **mm-memphy.c** | Physical memory: frame management, SWAP device I/O |
| **syscall.c** | Syscall dispatcher: route syscall numbers to handlers |
| **sys_*.c** | Individual syscall implementations (listsyscall, memmap, killall) |

---

## License

**Source Code License Grant:**

> The authors (HCMC University of Technology - Faculty of Computer Science & Engineering) hereby grant personal permission to use and modify the Licensed Source Code for the sole purpose of studying while attending the course CO2018 (Operating Systems) at HCMUT.

**Usage Rights:**
- ✅ Use for study purposes in CO2018
- ✅ Modify source code for assignment completion
- ✅ Run simulation and analyze results
- ❌ Redistribute without explicit permission
- ❌ Use for commercial purposes
- ❌ Remove or modify copyright notices

**For External Use:** Contact the course instructor or Faculty of Computer Science & Engineering for explicit permission.

---

## References & Resources

- **Assignment Specification:** `assignment.txt`
- **Header API Documentation:** `include/*.h`
- **Sample Outputs:** `output/` directory
- **Course:** CO2018 - Operating Systems, HCMC University of Technology

---

## Testing & Validation

### Test Execution
```bash
# Test individual configs
./os os_1_mlq_paging > test.out
diff output/os_1_mlq_paging.output test.out

# Run all tests
for test in input/os_*; do
    name=$(basename "$test")
    ./os "$name" > /tmp/out.txt
    if diff -q "output/${name}.output" /tmp/out.txt >/dev/null 2>&1; then
        echo "✓ $name PASS"
    else
        echo "✗ $name FAIL"
    fi
done
```

### Expected Output Events
- `Process X loaded` — Process successfully created and queued
- `CPU Y: Dispatched process X` — CPU assigned process to execute
- `CPU Y: Put process X to run queue` — Time slice expired; process requeued
- `CPU Y: Processed X has finished` — Process completed execution
- `CPU Y stopped` — No more processes to run

---

## Acknowledgments

### Original Authors & Instructors

This project is based on the **CO2018 Operating Systems** course at **Ho Chi Minh City University of Technology (HCMUT - VNU)**, Vietnam's leading institution for science and technology education.

- **Course:** [CO2018 — Operating Systems](https://www.hcmut.edu.vn)
- **Institution:** Ho Chi Minh City University of Technology - Faculty of Computer Science & Engineering
- **Instructor:** Hoang Le Hai Thanh
- **Laboratory:** HCMUT Operating Systems Lab

### Contributors

Team members who contributed to this implementation:

- [@Hilo22](https://github.com/Hilo22)
- [@khoinguyen248](https://github.com/khoinguyen248)
- [@MinhLe28042005](https://github.com/MinhLe28042005)
- [@hungnguyenviet-dev](https://github.com/hungnguyenviet-dev)

---

## 👤 Author

**Truong Trung Bao**  
Student, Ho Chi Minh City University of Technology (HCMUT — VNU)

- **GitHub:** [BrianTr9](https://github.com/BrianTr9)
- **University:** [Ho Chi Minh City University of Technology (HCMUT)](https://www.hcmut.edu.vn)
- **Course:** CO2018 — Operating Systems (Fall 2024)

---

**Version:** 3.0 (CO2018) | **Status:** Complete

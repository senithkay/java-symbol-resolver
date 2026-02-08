# eBPF CPU Spike Detector

An eBPF-based CPU spike detector that identifies processes causing CPU spikes and reports their **PID** and **instruction pointer (PC address)**. The output is designed to be fed directly into the [JVMTI-based Java symbol resolver](../agent.c) to resolve JIT-compiled code addresses back to Java method names.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Linux Kernel                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  eBPF program (attached to perf CPU clock @ 99 Hz)       │  │
│  │                                                           │  │
│  │  on each sample:                                          │  │
│  │    record (PID, instruction_pointer) → count              │  │
│  │    record PID → total_count                               │  │
│  └───────────────────────┬───────────────────────────────────┘  │
│                          │ BPF maps                              │
├──────────────────────────┼──────────────────────────────────────┤
│                          ▼           Userspace                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  spike_detector.py (BCC)                                  │  │
│  │                                                           │  │
│  │  every N seconds:                                         │  │
│  │    read BPF maps                                          │  │
│  │    if pid_total[pid] >= threshold → SPIKE                 │  │
│  │    emit: PID=<pid>  ADDR=0x<instruction_pointer>          │  │
│  └───────────────────────┬───────────────────────────────────┘  │
│                          │                                      │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  JVMTI agent (libagent.so) symbol table                   │  │
│  │                                                           │  │
│  │  lookup_symbol(0x<addr>)                                  │  │
│  │    → "heavyCompute" / "trigBurn" / "UNKNOWN"              │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## How It Works

1. An **eBPF program** is attached to the `PERF_COUNT_SW_CPU_CLOCK` software perf event, sampling at 99 Hz across all CPUs.
2. On each sample, the kernel-side eBPF code records the current process's **PID** and **userspace instruction pointer (PC)** into BPF hash maps.
3. Every `--interval` seconds, the userspace Python script reads the maps, identifies PIDs whose total sample count exceeds `--threshold`, and reports them as CPU spikes.
4. The reported `ADDR=0x...` values can be looked up in the JVMTI agent's symbol table (`lookup_symbol()`) to resolve JIT-compiled Java method names.

## Prerequisites

- **Linux kernel ≥ 4.15** (tested on 6.8)
- **Root privileges** (eBPF requires `CAP_BPF` / `CAP_PERFMON`)
- **BCC (BPF Compiler Collection)** with Python 3 bindings:
  ```bash
  sudo apt install bpfcc-tools python3-bpfcc
  ```
- **bpftrace** (for the lightweight `.bt` alternative):
  ```bash
  sudo apt install bpftrace
  ```
- **GCC** and **JDK** (for test programs):
  ```bash
  sudo apt install gcc default-jdk
  ```

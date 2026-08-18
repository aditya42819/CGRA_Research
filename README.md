# UltraFastMapper

A faithful C++17 baseline implementation of the **Diagonal CGRA Scheduling**
algorithm proposed in:

> J. Lee and T. E. Carlson,
> *Ultra-Fast CGRA Scheduling to Enable Run Time Programmable CGRAs*,
> DAC 2021.

This repository serves as the software baseline for future research on
hardware acceleration of CGRA mapping using AMD/Xilinx Vitis HLS.

The long-term goal is to investigate whether the scheduling algorithm can be
efficiently realized as dedicated FPGA hardware and to characterize the
resulting architectural bottlenecks.

---

## Features

- Faithful implementation of Algorithm 1
- Configurable CGRA dimensions
- Topological ordering
- RecMII and ResMII computation
- Diagonal placement strategy
- BFS-based routing
- Modulo Reservation Table (MRT)
- Schedule validation

---

## Build

```bash
cmake -S . -B build
cmake --build build
./build/ultrafast_mapper
```

---

## Algorithm Overview

```text
MinII = max(RecMII, ResMII)

for II = MinII .. MaxII:

    for each instruction in topological order:

        for each diagonal slot:

            PE = order[s / II]
            cycle = s
            slot  = s % II

            if all parent routes exist:

                reserve PE
                reserve links
                update MRT
                continue

            else

                try next slot

    if mapping failed

        II++

    else

        return schedule
```

---

## Current Status

This repository implements the baseline software scheduler exactly as described
in the paper.

Future work includes:

- Vitis HLS synthesis
- RTL generation
- Hardware profiling
- Bottleneck analysis
- Memory hierarchy optimization
- Routing microarchitecture optimization
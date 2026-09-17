# Lab 4: Parallel Reduction

## Overview

This lab covers **parallel reduction**, one of the most fundamental GPU programming patterns. Reduction operations — such as sum, max, min, and product — appear in countless algorithms from machine learning to scientific computing.

You will learn to implement and compare multiple reduction strategies, progressing from a naive atomic approach to highly optimized warp-shuffle techniques.

### Learning Objectives

1. Explain why naive atomic reduction is inefficient for large arrays
2. Implement tree-based parallel reduction using shared memory
3. Analyze the relationship between occupancy and performance
4. Understand warp-level optimizations (shuffle instructions)
5. Compare performance across four optimization strategies

## Problem Description

Implement a program that performs **parallel reduction (sum)** of an array of 32-bit floating point numbers on a GPU.

* External libraries are not permitted
* The final result must be stored in the output variable

### Input Format

```
N
a1 a2 ... aN
```

* 1 ≤ N ≤ 1,000,000,000
* aᵢ: floating point values

### Output Format

```
sum
```

### Example

**Input:**
```
5
1 2 3 4 5
```

**Output:**
```
15
```

## Optimization Strategies

The lab explores four progressively faster approaches:

| Strategy | Description |
|:---------|:------------|
| **Naive Atomic** | Every thread does `atomicAdd` to a single location — high contention |
| **Tree Reduction** | Shared-memory binary tree with `__syncthreads()` — O(log N) steps |
| **Tree + Warp Shuffle** | Final warp uses `__shfl_down` instead of shared memory — no sync needed |
| **Multi-Element + Tree + Warp** | Each thread sums multiple elements before tree reduction — best bandwidth |

## How to Run

### 1. Generate Test Cases

```bash
python3 geninput.py --quick              # Small + Medium only (1.in, 2.in)
python3 geninput.py                      # Add 3.in (100M elements)
python3 geninput.py --include-test4      # Add 4.in (1B elements, slow)
python3 geninput.py --chunk-size 500000  # Tune memory usage per write chunk
```

| Test Case | N | Description |
|:----------|:--|:------------|
| `1.in` | 10 | Quick sanity check |
| `2.in` | 1,000,000 | Medium benchmark |
| `3.in` | 100,000,000 | Large-scale test |
| `4.in` | 1,000,000,000 | Stress test (opt-in) |

### 2. Compile

```bash
# Naive atomic reference implementation
hipcc -O2 fs_main.hip -o exe_reduction

# Benchmark (all 4 strategies)
hipcc -O2 benchmark.hip -o exe_benchmark
```

Or use the Makefile:

```bash
make                            # Build all targets
make GPU_ARCH=gfx1100           # Specify GPU architecture
make clean                      # Remove executables and testcases
```

### 3. Run

```bash
# Run naive reduction
./exe_reduction testcases/2.in

# Run benchmark comparing all strategies
./exe_benchmark testcases/2.in

# Time the execution
time ./exe_reduction testcases/3.in
```

## Lab Files

| File | Description |
|:-----|:------------|
| `main.hip` | Student implementation (start here) |
| `fs_main.hip` | Reference naive atomic implementation with file I/O |
| `benchmark.hip` | All 4 strategies with GPU timing and comparison |
| `geninput.py` | Test case generator (streaming, low-memory) |
| `Makefile` | Build configuration |
| `Lab4_Reduction.ipynb` | Interactive lab notebook with exercises |
| `README.md` | This file |

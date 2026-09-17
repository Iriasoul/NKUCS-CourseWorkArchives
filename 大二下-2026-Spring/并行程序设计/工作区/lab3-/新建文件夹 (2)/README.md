# Lab 3 - Pthread/OpenMP 多线程并行 (ANN 选题)

在 Lab 2 SIMD 实验基础上, 使用 Pthread/OpenMP 进行多线程并行优化.

## 文件结构

```
lab3/
├── main.cc          # 主测试程序 (12 个策略的统一入口)
├── flat_thread.h    # 策略 1, 2:  Flat-SIMD + OpenMP / Pthread
├── pq_thread.h      # 策略 3-6:   PQ 相关 (含 PQ 离线训练 + 4 个查询策略)
├── ivf.h            # 策略 7-9:   IVF baseline + OpenMP + Pthread
└── ivf_pq.h         # 策略 10-12: IVF-PQ baseline + OpenMP + Pthread
```

## 12 个策略

| 策略 | 名称                       | 在线并行  | 说明                              |
|----|--------------------------|-------|---------------------------------|
| 1  | Flat-SIMD + OpenMP       | OpenMP   | base 分块并行 + 局部 top-k             |
| 2  | Flat-SIMD + Pthread      | Pthread  | 手动线程, 用于与 OpenMP 等价性对比             |
| 3  | PQ-Rerank-SIMD baseline  | 单线程   | Lab2 策略 4 风格 (逐 centroid + AoS)  |
| 4  | PQ-Final baseline        | 单线程   | 跨 centroid SIMD + SoA, 不含多线程     |
| 5  | PQ-SIMD + OpenMP         | OpenMP   | 在 4 基础上加 OpenMP                  |
| 6  | PQ-SIMD + Pthread        | Pthread  | 用 pthread_barrier 显式同步           |
| 7  | IVF-SIMD baseline        | 单线程   | KMeans + 内存重排                    |
| 8  | IVF-SIMD + OpenMP        | OpenMP   | 簇级 dynamic 调度                   |
| 9  | IVF-SIMD + Pthread       | Pthread  | 手动实现动态任务队列 (mutex + 计数器)         |
| 10 | IVF-PQ-SIMD baseline     | 单线程   | IVF + PQ 嵌套近似                    |
| 11 | IVF-PQ-SIMD + OpenMP     | OpenMP   | 完整并行流水线                          |
| 12 | IVF-PQ-SIMD + Pthread    | Pthread  | barrier + mutex 双同步原语            |

## 使用方法

### 准备

把 5 个文件 (`main.cc`, `flat_thread.h`, `pq_thread.h`, `ivf.h`, `ivf_pq.h`)
放到 lab2 框架的同一目录下, 与 `flat_scan.h`, `hnswlib/` 同级.

### 切换策略

编辑 `main.cc` 顶部的几个变量:

```cpp
int strategy           = 12;   // 1..12
int rerank_k           = 125;  // PQ rerank 用 (策略 3-6, 10-12)
int nprobe             = 16;   // IVF 用 (策略 7-12)
int nlist              = 256;  // IVF 簇数
int online_omp_threads = 4;    // 在线 OpenMP 策略 (1, 5, 8, 11) 用
int num_pthreads       = 4;    // 在线 Pthread 策略 (2, 6, 9, 12) 用
```

### 编译

用框架的 `test.sh`, 或者手动:

```bash
g++ -O3 -fopenmp -march=armv8-a -ffast-math main.cc -lpthread -o main
./main
```

## 关键设计要点

### 离线/在线分离

- **离线建索引**: 所有策略统一用 OpenMP 4 线程, 由
  `omp_set_num_threads(OFFLINE_OMP_THREADS)` 在 main 开头固定, 不参与对比讨论.
- **在线查询**: 通过 `#pragma omp parallel num_threads(nthr)` 子句**显式覆盖**,
  完全独立于离线设置. 调整 `online_omp_threads` 不影响离线训练.
- **所有 latency 测量均仅包含在线查询**, 不计入离线建索引耗时.

### 局部 top-k + 全局归并 (反锁竞争范式)

所有多线程策略都采用这一设计:
- 每个线程维护自己的局部优先队列, 完全无锁;
- 扫描完成后, 把 T × k (或 T × rerank_k) 个元素归并为全局结果;
- 归并代价远小于扫描代价, 几乎免费.

### 负载均衡

- **均匀任务** (Flat 全库扫描) → `schedule(static)`
- **不均匀任务** (IVF 簇级扫描, 簇大小差异大) → `schedule(dynamic, 1)`
- Pthread 版本中, dynamic 调度用 `pthread_mutex_t` + 共享计数器手工实现

### 多阶段流水线

PQ / IVF-PQ 涉及多阶段 (LUT 构建 / ADC 粗排 / 精排):
- OpenMP 用 `nowait` 子句消除阶段间隐式 barrier
- Pthread 用 `pthread_barrier_t` 在必要处显式同步, 不需要时不同步

## 策略对比的清晰演化链

| 演化 | 增量来源 |
|---|---|
| 3 → 4 | 纯 SIMD (跨 centroid + SoA), 不含多线程 |
| 4 → 5 | OpenMP 多线程 |
| 4 → 6 | Pthread 多线程 |
| 7 → 8 | IVF 簇级 OpenMP dynamic |
| 7 → 9 | IVF 簇级 Pthread 手动动态调度 |
| 10 → 11 | IVF-PQ 完整 OpenMP 流水线 |
| 10 → 12 | IVF-PQ 完整 Pthread 流水线 |

每个 baseline → 多线程的对比都干净, 多线程加速比可以独立测量.

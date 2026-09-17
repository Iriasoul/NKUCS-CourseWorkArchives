#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
批量运行测评:对 manifest.csv 中每个实例调用 ./bench,汇总成 results.csv。

用法:  python3 run_bench.py
先确保已 make(需要 ./bench)且已运行 gen_sweep.py(需要 manifest.csv)。
计时相关结果(ms_min/ms_mean)依赖本机,请在你的目标主机上运行本脚本。
"""
import csv, platform, subprocess

# ===================== 运行参数(按需修改)=====================
REPS        = 5                       # 每个计时点重复次数(取 min/mean)
THREADS     = 4                       # 并行线程数(strategy=3),设为你的物理核数
EPS_LIST    = "0,0.02,0.05,0.10,0.20" # 近似容差扫描
BRUTE_MAX_K = 10                      # 暴力仅在 k<=此值时运行(小规模正确性基准)
# =============================================================

BENCH_COLS = ["method", "eps", "cost", "ratio", "evaluated", "pruned",
              "ms_min", "ms_mean", "feasible", "k"]

def exe(name):
    return name + ".exe" if platform.system() == "Windows" else "./" + name

def main():
    bench = exe("bench")
    with open("manifest.csv", newline="", encoding="utf-8") as f:
        man = list(csv.DictReader(f))
    out_rows = []
    n = len(man)
    for i, m in enumerate(man, 1):
        r = subprocess.run([bench, m["file"], str(REPS), str(THREADS),
                            EPS_LIST, str(BRUTE_MAX_K)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print(f"  [跳过] {m['file']}: {r.stderr.strip()}"); continue
        for line in r.stdout.strip().splitlines():
            v = line.split(",")
            if len(v) != len(BENCH_COLS): continue
            rec = dict(zip(BENCH_COLS, v))
            rec.update(file=m["file"], dist=m["dist"], tight=m["tight"],
                       seed=m["seed"], opt=m["opt"])
            out_rows.append(rec)
        if i % 25 == 0 or i == n:
            print(f"  测评进度 {i}/{n}")
    cols = ["file", "k", "dist", "tight", "seed", "opt"] + BENCH_COLS[:-1]
    with open("results.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader(); w.writerows(out_rows)
    print(f"完成:{len(out_rows)} 行 -> results.csv")

if __name__ == "__main__":
    main()

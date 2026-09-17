#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
批处理生成器:按 因子网格(规模 k × 空间分布 × 时窗松紧 × 随机种子)生成实例,
每个实例文件内嵌 # OPT 标准答案,并汇总写出 manifest.csv。

用法:  python3 gen_sweep.py
先确保已 make(需要 ./gen)。生成的实例放在 instances/ 下。
"""
import os, csv, platform, subprocess

# ===================== 因子网格(按需修改)=====================
KS     = [8, 10, 12, 14, 16, 18]
DISTS  = ["uniform", "clustered", "linear", "spiral", "sparse"]
TIGHTS = ["loose", "tight"]
SEEDS  = [0, 1, 2, 3, 4]            # 每个设计点的种子数(刻画实例间差异)
OUTDIR = "instances"
# =============================================================

def exe(name):
    return name + ".exe" if platform.system() == "Windows" else "./" + name

def read_opt(path):
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if s.startswith("# OPT"):
                try: return float(s.split()[2])
                except Exception: return ""
    return ""

def main():
    gen = exe("gen")
    os.makedirs(OUTDIR, exist_ok=True)
    rows = []
    total = len(KS) * len(DISTS) * len(TIGHTS) * len(SEEDS)
    i = 0
    for k in KS:
        for dist in DISTS:
            for tight in TIGHTS:
                for seed in SEEDS:
                    i += 1
                    fname = f"k{k:02d}_{dist}_{tight}_s{seed}.txt"
                    fpath = os.path.join(OUTDIR, fname)
                    r = subprocess.run([gen, fpath, str(k), dist, tight, str(seed)],
                                       capture_output=True, text=True)
                    if r.returncode != 0:
                        print(f"  [跳过] {fname}: {r.stderr.strip()}")
                        continue
                    opt = read_opt(fpath)
                    rows.append(dict(file=fpath, k=k, dist=dist, tight=tight,
                                     seed=seed, opt=opt))
                    if i % 25 == 0 or i == total:
                        print(f"  生成进度 {i}/{total}")
    with open("manifest.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["file", "k", "dist", "tight", "seed", "opt"])
        w.writeheader(); w.writerows(rows)
    print(f"完成:{len(rows)} 个实例 -> {OUTDIR}/,清单 -> manifest.csv")

if __name__ == "__main__":
    main()

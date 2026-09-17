#!python
# -*- coding: utf-8 -*-

import os
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

OUT = "analysis"
APPROX_REF = 0.10            # 增长曲线里近似曲线用的代表性容差(本脚本未画近似增长曲线,保留以备用)
SCALING_CSV = "scaling.csv"  # 并行强扩展性数据(threads, ms_min[, ms_mean])

# 总览双图的两个代表性实例(None=自动挑选;也可指定文件名以复现报告图)
REP_SMALL = "instances/k10_clustered_loose_s0.txt"   # 小规模(自动:有暴力解且 k 最大者)
REP_LARGE = "instances/k16_clustered_tight_s0.txt"   # 较大规模(自动:k>=14 中剪枝提速最大者)

NAME = {"brute": "Brute", "dp": "DP", "dp_par": "DP-parallel", "dp_prune": "DP-prune"}


def gmean(a):
    a = np.asarray(a, float); a = a[a > 0]
    return float(np.exp(np.mean(np.log(a)))) if len(a) else float("nan")


def load():
    df = pd.read_csv("results.csv")
    for c in ["k", "eps", "cost", "ratio", "evaluated", "pruned",
              "ms_min", "ms_mean", "feasible", "opt", "seed"]:
        if c in df:
            df[c] = pd.to_numeric(df[c], errors="coerce")
    return df


# 取某方法(approx 需指定 eps)的逐实例表,索引为 file
def slab(df, method, eps=None):
    s = df[df["method"] == method]
    if eps is not None:
        s = s[np.isclose(s["eps"], eps)]
    return s.set_index("file")


# ==================== fig:overview ====================
def pick_small(df):
    if REP_SMALL is not None and (df["file"] == REP_SMALL).any():
        return REP_SMALL
    b = df[(df["method"] == "brute") & (df["feasible"] == 1)]
    pool = b if len(b) else df[df["feasible"] == 1]
    if not len(pool):
        return None
    kmax = pool["k"].max()
    return pool[pool["k"] == kmax].sort_values(["dist", "tight", "seed"]).iloc[0]["file"]


def pick_large(df):
    if REP_LARGE is not None and (df["file"] == REP_LARGE).any():
        return REP_LARGE
    dp = slab(df, "dp")["ms_min"].rename("dp_ms")
    pr = slab(df, "dp_prune")[["ms_min", "k"]].rename(columns={"ms_min": "pr_ms"})
    j = pr.join(dp, how="inner")
    j = j[(j["k"] >= 14) & (j["dp_ms"] > 0) & (j["pr_ms"] > 0)]
    if not len(j):
        return None
    j["sp"] = j["dp_ms"] / j["pr_ms"]
    return j["sp"].idxmax()


def _panel(df, f, methods):
    sub = df[df["file"] == f]
    if not len(sub):
        return None
    labels, vals = [], []
    for m in methods:
        r = sub[sub["method"] == m]
        if len(r):
            labels.append(NAME[m]); vals.append(float(r.iloc[0]["ms_mean"]))
    if not labels:
        return None
    meta = sub.iloc[0]
    return dict(labels=labels, vals=vals, k=int(meta["k"]),
                dist=meta["dist"], tight=meta["tight"])


def overview(df):
    fs, fl = pick_small(df), pick_large(df)
    specs = []
    if fs is not None:
        d = _panel(df, fs, ["brute", "dp", "dp_par", "dp_prune"])
        if d:
            specs.append(d)
    if fl is not None and fl != fs:
        d = _panel(df, fl, ["dp", "dp_par", "dp_prune"])
        if d:
            specs.append(d)
    if not specs:
        return

    fig, axes = plt.subplots(1, len(specs), figsize=(5.5 * len(specs), 4.2))
    if len(specs) == 1:
        axes = [axes]
    for ax, d in zip(axes, specs):
        xs = np.arange(len(d["vals"]))
        ax.bar(xs, d["vals"], color="#4c72b0", alpha=0.85)
        for x, v in zip(xs, d["vals"]):
            ax.text(x, v, f"{v:.3g}", ha="center", va="bottom", fontsize=8)
        ax.set_yscale("log")
        ax.set_xticks(xs); ax.set_xticklabels(d["labels"], rotation=15)
        ax.set_title(f"k={d['k']}  {d['dist']} / {d['tight']}")
        ax.grid(True, axis="y", which="both", alpha=0.3)
    axes[0].set_ylabel("average latency ms (mean, log)")
    fig.suptitle("Average latency by method (representative instances)")
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "overview_latency.png"), dpi=130); plt.close(fig)


# ==================== fig:speedup(需 scaling.csv) ====================
def parallel_scaling():
    if not os.path.exists(SCALING_CSV):
        print(f"[speedup.png 跳过] 未找到 {SCALING_CSV}(并行强扩展性数据,需自行生成)")
        return
    sc = pd.read_csv(SCALING_CSV)
    sc["threads"] = pd.to_numeric(sc["threads"], errors="coerce")
    tcol = "ms_min" if "ms_min" in sc else ("ms_mean" if "ms_mean" in sc else None)
    if tcol is None:
        print(f"[speedup.png 跳过] {SCALING_CSV} 缺少 ms_min/ms_mean 列")
        return
    sc[tcol] = pd.to_numeric(sc[tcol], errors="coerce")
    sc = sc.dropna(subset=["threads", tcol]).sort_values("threads")
    if not len(sc):
        print(f"[speedup.png 跳过] {SCALING_CSV} 无有效行")
        return

    base_t, base_ms = sc.iloc[0]["threads"], sc.iloc[0][tcol]
    sc["speedup"] = base_ms / sc[tcol]
    sc["efficiency"] = sc["speedup"] / (sc["threads"] / base_t)

    fig, ax1 = plt.subplots(figsize=(7, 4.2))
    thr = sc["threads"].values
    ax1.plot(thr, sc["speedup"].values, "o-", color="#1f77b4", label="speedup")
    ax1.plot(thr, thr / base_t, "k--", alpha=0.7, label="ideal (linear)")
    ax1.set_xlabel("threads"); ax1.set_ylabel(f"speedup (vs {int(base_t)} thread)")
    ax1.grid(True, alpha=0.3)
    ax2 = ax1.twinx()
    ax2.plot(thr, sc["efficiency"].values * 100, "s--", color="#ff7f0e", label="efficiency")
    ax2.set_ylabel("parallel efficiency %"); ax2.set_ylim(0, 110)
    h1, l1 = ax1.get_legend_handles_labels()
    h2, l2 = ax2.get_legend_handles_labels()
    ax1.legend(h1 + h2, l1 + l2, fontsize=8, loc="best")
    ax1.set_title("Parallel DP strong scaling")
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "speedup.png"), dpi=130); plt.close(fig)


# ==================== fig:states / fig:time(增长曲线,中位数+IQR,分松紧) ====================
def growth(df, value, fname, ylabel, title, methods):
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
    for ax, tight in zip(axes, ["loose", "tight"]):
        for m in methods:
            s = df[(df["method"] == m) & (df["tight"] == tight)]
            if not len(s):
                continue
            g = s.groupby("k")[value]
            med, q1, q3 = g.median(), g.quantile(.25), g.quantile(.75)
            ax.plot(med.index, med.values, "o-", label=NAME[m])
            ax.fill_between(med.index, q1.values, q3.values, alpha=0.2)
        ax.set_yscale("log")
        ax.set_title(tight); ax.set_xlabel("k"); ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel(ylabel); axes[0].legend(fontsize=8)
    fig.suptitle(title); fig.tight_layout()
    fig.savefig(os.path.join(OUT, fname), dpi=130); plt.close(fig)


# ==================== fig:reduction ====================
def reduction(df):
    dp = slab(df, "dp")["evaluated"]
    pr = slab(df, "dp_prune")[["evaluated", "tight", "dist"]]
    j = pr.join(dp.rename("dp_eval"), how="inner")
    j = j[j["dp_eval"] > 0]
    j["reduction"] = (1 - j["evaluated"] / j["dp_eval"]) * 100

    fig, ax = plt.subplots(figsize=(8, 4.2))
    order = sorted(j["dist"].unique())
    pos = np.arange(len(order))
    for off, t, col in [(-0.18, "loose", "#1f77b4"), (0.18, "tight", "#ff7f0e")]:
        vals = [j[(j["dist"] == d) & (j["tight"] == t)]["reduction"].values for d in order]
        bp = ax.boxplot(vals, positions=pos + off, widths=0.32, patch_artist=True, showfliers=False)
        for b in bp["boxes"]:
            b.set(facecolor=col, alpha=0.5)
    ax.set_xticks(pos); ax.set_xticklabels(order)
    ax.set_ylabel("state reduction % (1 - prune/DP)"); ax.set_xlabel("distribution")
    ax.set_title("Pruning reduction by distribution & time-window tightness")
    ax.legend(handles=[Patch(facecolor="#1f77b4", alpha=0.5, label="loose"),
                       Patch(facecolor="#ff7f0e", alpha=0.5, label="tight")], fontsize=8)
    ax.grid(True, axis="y", alpha=0.3)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "reduction.png"), dpi=130); plt.close(fig)


# ==================== fig:gap ====================
def approx_gap(df):
    a = df[(df["method"] == "approx") & (df["feasible"] == 1) & (df["ratio"] > 0)].copy()
    if not len(a):
        return
    a["gap"] = (a["ratio"] - 1) * 100
    epss = sorted(a["eps"].unique())

    fig, ax = plt.subplots(figsize=(7, 4.2))
    jit = (np.random.default_rng(0).random(len(a)) - .5) * 0.006
    ax.scatter(a["eps"] + jit, a["gap"], s=8, alpha=0.3, color="#1f77b4", label="actual gap (per instance)")
    mx = a.groupby("eps")["gap"].max()
    ax.plot(mx.index, mx.values, "o-", color="#d62728", label="max actual gap (envelope)")
    ax.plot(epss, [e * 100 for e in epss], "k--", label="guarantee = 100*eps")
    ax.set_xlabel("eps"); ax.set_ylabel("optimality gap %")
    ax.set_title("Actual gap stays under (1+eps) guarantee")
    ax.legend(fontsize=8); ax.grid(True, alpha=0.3)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "approx_gap.png"), dpi=130); plt.close(fig)


# ==================== fig:approxspeed ====================
def approx_speedup(df):
    dp = slab(df, "dp")["ms_min"].rename("dp_ms")
    a = df[(df["method"] == "approx") & (df["feasible"] == 1)].copy()
    a = a.join(dp, on="file")
    a = a[(a["dp_ms"] > 0) & (a["ms_min"] > 0)]
    if not len(a):
        return
    a["sp"] = a["dp_ms"] / a["ms_min"]                 # 相对串行 DP 的延迟加速比(ms_min)

    fig, ax = plt.subplots(figsize=(7, 4.2))
    style = {"loose": ("#1f77b4", "o-"), "tight": ("#ff7f0e", "s--")}
    for t in ["loose", "tight"]:
        s = a[a["tight"] == t]
        if not len(s):
            continue
        g = s.groupby("eps")["sp"].apply(gmean)        # 同 eps 下跨实例几何平均
        col, ls = style[t]
        ax.plot(g.index, g.values, ls, color=col, label=t)
    gall = a.groupby("eps")["sp"].apply(gmean)
    ax.plot(gall.index, gall.values, "^:", color="#555555", label="all")
    ax.set_xlabel("eps"); ax.set_ylabel("speedup vs serial DP (geomean, ms_min)")
    ax.set_title("Approximation latency speedup grows with tolerance")
    ax.legend(fontsize=8); ax.grid(True, alpha=0.3)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "approx_speedup.png"), dpi=130); plt.close(fig)


def main():
    os.makedirs(OUT, exist_ok=True)
    df = load()
    jobs = [
        ("overview_latency.png", lambda: overview(df)),
        ("speedup.png",          lambda: parallel_scaling()),
        ("states_growth.png",    lambda: growth(df, "evaluated", "states_growth.png",
                                                "expanded states (median, log)",
                                                "State growth vs k (DP vs DP-prune, IQR band)",
                                                ["dp", "dp_prune"])),
        ("time_growth.png",      lambda: growth(df, "ms_min", "time_growth.png",
                                                "time ms (min, median, log)",
                                                "Time growth vs k (host-dependent, IQR band)",
                                                ["dp", "dp_prune"])),
        ("reduction.png",        lambda: reduction(df)),
        ("approx_gap.png",       lambda: approx_gap(df)),
        ("approx_speedup.png",   lambda: approx_speedup(df)),
    ]
    for name, fn in jobs:
        try:
            fn()
        except Exception as e:
            print(f"[{name} 跳过] {e}")
    print(f"图已输出 -> {OUT}/")


if __name__ == "__main__":
    main()
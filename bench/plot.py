"""把 bench 导出的逐笔样本画成图。

用法：
    ./build/order_book_bench insert 100000 42 samples.csv
    ./build/order_book_bench cancel 100000 42 samples.csv
    ./build/order_book_bench mixed  100000 42 samples.csv
    python bench/plot.py samples.csv bench/latency.png

左图是累积分布（CDF），横轴对数——回答「多少比例的操作在 X 纳秒内完成」。
右图是直方图，横轴对数——回答「尾部是断崖还是长坡」。

两个图放在一起是因为它们看的东西不同：CDF 看分位点，直方图看形状。
单看 p99 一个数字，会漏掉「分布是不是双峰」这类结构性问题。
"""

import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# 中文字体：直接找系统里带 CJK 的字体文件，逐个尝试注册。
# 找不到就退回默认字体（中文会显示成方框，但图仍能出）。
def _setup_cjk_font():
    from matplotlib import font_manager
    for path in font_manager.findSystemFonts():
        if "CJK" in path and path.endswith((".ttc", ".otf", ".ttf")):
            try:
                font_manager.fontManager.addfont(path)
                name = font_manager.FontProperties(fname=path).get_name()
                plt.rcParams["font.family"] = name
                return
            except Exception:
                continue

_setup_cjk_font()
plt.rcParams["axes.unicode_minus"] = False
import numpy as np

LABELS = {
    "insert": "insert（放单）",
    "cancel": "cancel（撤单）",
    "match": "cross+match（撮合）",
}
COLORS = {"insert": "#2b6cb0", "cancel": "#c05621", "match": "#2f855a"}


def load(path):
    data = defaultdict(list)
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or "," not in line:
                continue
            name, value = line.split(",", 1)
            data[name].append(float(value))
    return {k: np.array(v) for k, v in data.items()}


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "samples.csv"
    dst = sys.argv[2] if len(sys.argv) > 2 else "bench/latency.png"

    data = load(src)
    if not data:
        print("没有样本，先跑 bench 并把第 4 个参数设成输出文件")
        return 1

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4.6), dpi=140)

    # ---------- 左：CDF ----------
    for name, arr in sorted(data.items()):
        arr = np.sort(arr)
        cdf = np.arange(1, arr.size + 1) / arr.size * 100.0
        ax1.plot(arr, cdf, color=COLORS.get(name, "gray"), linewidth=1.6,
                 label=f"{LABELS.get(name, name)}  n={arr.size:,}")

    ax1.set_xscale("log")
    ax1.set_xlabel("单笔延迟（ns，对数轴）")
    ax1.set_ylabel("已完成比例（%）")
    ax1.set_title("延迟累积分布", fontsize=11)
    ax1.grid(True, which="both", alpha=0.25, linewidth=0.6)
    ax1.axhline(99, color="black", linestyle=":", linewidth=0.9, alpha=0.6)
    ax1.axhline(99.9, color="black", linestyle="--", linewidth=0.9, alpha=0.5)
    xr = ax1.get_xlim()
    ax1.text(xr[1] * 0.55, 99.6, "p99", fontsize=8, color="0.35")
    ax1.text(xr[1] * 0.55, 100.6, "p99.9", fontsize=8, color="0.35")
    ax1.set_ylim(0, 102)
    ax1.legend(loc="lower right", fontsize=8, frameon=False)

    # ---------- 右：直方图 ----------
    all_vals = np.concatenate([v for v in data.values() if v.size])
    lo = max(1.0, np.percentile(all_vals, 0.5))
    hi = np.percentile(all_vals, 99.99)
    bins = np.logspace(np.log10(lo), np.log10(max(hi, lo * 10)), 50)

    for name, arr in sorted(data.items()):
        ax2.hist(arr, bins=bins, histtype="step", linewidth=1.5,
                 color=COLORS.get(name, "gray"), label=LABELS.get(name, name))

    ax2.set_xscale("log")
    ax2.set_xlabel("单笔延迟（ns，对数轴）")
    ax2.set_ylabel("样本数")
    ax2.set_title("延迟分布形状（截到 p99.99）", fontsize=11)
    ax2.grid(True, which="both", alpha=0.25, linewidth=0.6)
    ax2.legend(fontsize=8, frameon=False)

    fig.suptitle("Order Book v3 延迟基准（x86-64, g++ -O2, 单次计时含约 20 ns 计时器开销）",
                 fontsize=10, y=0.99)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(dst)
    print(f"saved {dst}")

    # 顺手把关键分位数打到终端，方便贴 README
    for name, arr in sorted(data.items()):
        q = np.percentile(arr, [50, 90, 99, 99.9])
        print(f"{name:8s} p50={q[0]:7.0f}  p90={q[1]:7.0f}  p99={q[2]:8.0f}  p999={q[3]:9.0f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

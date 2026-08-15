#!/usr/bin/env python3
"""
测试报告可视化脚本
读取 stability_report.csv，生成多子图报告（PNG）。

用法：
    python3 scripts/plot_report.py [csv文件路径] [输出图片路径]

示例：
    python3 scripts/plot_report.py cmake-build-debug/bin/stability_report.csv report.png
    python3 scripts/plot_report.py                          # 使用默认路径

依赖：
    pip install matplotlib
"""

import sys
import csv
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")  # 无头模式，不依赖显示服务器
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
    from matplotlib import font_manager
    from datetime import datetime
except ImportError:
    print("缺少依赖，请先安装：pip install matplotlib", file=sys.stderr)
    sys.exit(1)

# 配置字体：优先选支持中文的字体，找不到则回退默认（中文显示为方块但不影响数据）
_cjk_fonts = [
    "PingFang SC", "Heiti TC", "STHeiti", "Microsoft YaHei",
    "Noto Sans CJK SC", "WenQuanYi Zen Hei", "Arial Unicode MS",
]
_available = {f.name for f in font_manager.fontManager.ttflist}
_chosen_font = next((f for f in _cjk_fonts if f in _available), None)
if _chosen_font:
    plt.rcParams["font.family"] = [_chosen_font]
plt.rcParams["axes.unicode_minus"] = False  # 解决负号显示为方块的问题


def load_csv(path: str) -> dict:
    """读取 CSV，返回各列数据的字典。"""
    cols = {
        "timestamp": [], "count": [], "p50_us": [], "p99_us": [],
        "avg_us": [], "errors": [], "error_rate": []
    }
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for k in cols:
                cols[k].append(float(row[k]))
    return cols


def compute_qps(counts: list, timestamps: list) -> tuple:
    """用相邻两行的 count 差 / 时间差算出瞬时 QPS 和时间点。"""
    qps_times = []
    qps_values = []
    for i in range(1, len(counts)):
        dt = timestamps[i] - timestamps[i - 1]
        if dt <= 0:
            continue
        qps_times.append(timestamps[i])
        qps_values.append((counts[i] - counts[i - 1]) / dt)
    return qps_times, qps_values


def plot_report(csv_path: str, output_path: str) -> None:
    data = load_csv(csv_path)
    ts = data["timestamp"]
    if not ts:
        print(f"CSV 无数据: {csv_path}", file=sys.stderr)
        sys.exit(1)

    # Unix 时间戳转 datetime 用于 X 轴显示
    times = [datetime.fromtimestamp(t) for t in ts]
    qps_times, qps_values = compute_qps(data["count"], ts)

    fig, axes = plt.subplots(4, 1, figsize=(12, 14), sharex="col")
    fig.suptitle(f"Stability Test Report\n{Path(csv_path).name}", fontsize=14, fontweight="bold")

    # ── 1. 延迟时序图 ──────────────────────────────────────────────
    ax = axes[0]
    ax.plot(times, data["p50_us"], label="P50", color="#2196F3", linewidth=1.5)
    ax.plot(times, data["p99_us"], label="P99", color="#F44336", linewidth=1.5)
    ax.plot(times, data["avg_us"], label="Avg", color="#4CAF50", linewidth=1.2, linestyle="--")
    ax.set_ylabel("Latency (µs)")
    ax.set_title("Latency Over Time")
    ax.legend(loc="upper left")
    ax.grid(True, alpha=0.3)
    # 用科学计数法避免大数字重叠
    ax.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))

    # ── 2. 吞吐量（QPS）时序图 ────────────────────────────────────────
    ax = axes[1]
    if qps_values:
        qps_dt = [datetime.fromtimestamp(t) for t in qps_times]
        ax.plot(qps_dt, qps_values, color="#FF9800", linewidth=1.5)
    ax.set_ylabel("QPS (req/s)")
    ax.set_title("Throughput Over Time")
    ax.grid(True, alpha=0.3)

    # ── 3. 错误率时序图 ──────────────────────────────────────────────
    ax = axes[2]
    ax.fill_between(times, [r * 100 for r in data["error_rate"]],
                     color="#F44336", alpha=0.4, step="mid")
    ax.plot(times, [r * 100 for r in data["error_rate"]],
            color="#F44336", linewidth=1.2)
    ax.set_ylabel("Error Rate (%)")
    ax.set_title("Error Rate Over Time")
    ax.grid(True, alpha=0.3)
    ax.set_ylim(bottom=0)

    # ── 4. 累计请求 / 累计错误 ─────────────────────────────────────────
    ax = axes[3]
    ax.plot(times, data["count"], label="Total Requests", color="#2196F3", linewidth=1.5)
    ax_win = ax.twinx()
    ax_win.plot(times, data["errors"], label="Total Errors", color="#F44336", linewidth=1.2, linestyle="--")
    ax.set_ylabel("Total Requests", color="#2196F3")
    ax_win.set_ylabel("Total Errors", color="#F44336")
    ax.set_title("Cumulative Requests & Errors")
    ax.grid(True, alpha=0.3)
    # 合并两个 Y 轴的图例
    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax_win.get_legend_handles_labels()
    ax.legend(lines1 + lines2, labels1 + labels2, loc="upper left")

    # 底部 X 轴共享
    axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
    axes[-1].xaxis.set_major_locator(mdates.AutoDateLocator())
    plt.gcf().autofmt_xdate()

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"报告已保存: {output_path}")

    # 打印摘要统计
    print(f"\n=== 摘要 ===")
    print(f"测试时长:       {ts[-1] - ts[0]:.0f} 秒")
    print(f"总请求数:       {int(data['count'][-1])}")
    print(f"最终错误率:     {data['error_rate'][-1]*100:.3f}%")
    print(f"P50 延迟(末值): {data['p50_us'][-1]:.0f} µs")
    print(f"P99 延迟(末值): {data['p99_us'][-1]:.0f} µs")
    if qps_values:
        print(f"平均 QPS:       {sum(qps_values)/len(qps_values):.1f}")


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "cmake-build-debug/bin/stability_report.csv"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "cmake-build-debug/bin/stability_report.png"

    if not Path(csv_path).exists():
        print(f"CSV 文件不存在: {csv_path}", file=sys.stderr)
        print("用法: python3 scripts/plot_report.py <csv路径> [输出png路径]", file=sys.stderr)
        sys.exit(1)

    plot_report(csv_path, output_path)


if __name__ == "__main__":
    main()

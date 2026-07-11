#!/bin/bash
# 性能基线采集 + Profiler 运行脚本
#
# 用法：
#   ./scripts/run_perf.sh                # 仅跑性能测试
#   ./scripts/run_perf.sh --perf         # perf record 采样 CPU（Linux）
#   ./scripts/run_perf.sh --heaptrack    # heaptrack 采样堆内存（Linux）
#   ./scripts/run_perf.sh --sample       # sample 采样 CPU（macOS）
#
# 输出：
#   cmake-build-debug/bin/perf_report.csv    — 阶梯施压数据
#   <output_dir>/perf.data                    — perf 结果（--perf, Linux）
#   <output_dir>/heaptrack.IMTest.*.gz        — heaptrack 结果（--heaptrack, Linux）
#   <output_dir>/sample.txt                   — sample 结果（--sample, macOS）

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

IMTEST="./cmake-build-debug/bin/IMTest"
OUTPUT_DIR="./cmake-build-debug/bin"
MODE="none"

# ── 解析参数 ─────────────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --perf)
            MODE="perf"
            echo "=== 将使用 perf record 采样 CPU（Linux）==="
            ;;
        --heaptrack)
            MODE="heaptrack"
            echo "=== 将使用 heaptrack 采样堆内存（Linux）==="
            ;;
        --sample)
            MODE="sample"
            echo "=== 将使用 sample 采样 CPU（macOS）==="
            ;;
        *)
            echo "未知参数: $arg"
            echo "用法: $0 [--perf|--heaptrack|--sample]"
            exit 1
            ;;
    esac
done

# 确保 libssl 能找到（macOS）
export DYLD_LIBRARY_PATH="/usr/local/mysql-connector-c++-9.7.0/lib64:/opt/homebrew/lib:${DYLD_LIBRARY_PATH:-}"

# ── 运行 ─────────────────────────────────────────────────────────────────────
echo "=== 运行性能测试套件 ==="

case "$MODE" in
    perf)
        perf record -g -o "${OUTPUT_DIR}/perf.data" \
            "$IMTEST" --gtest_filter="PerfTest.*"
        ;;
    heaptrack)
        heaptrack -o "${OUTPUT_DIR}/heaptrack" \
            "$IMTEST" --gtest_filter="PerfTest.*"
        ;;
    sample)
        # sample 需要 attach 到已运行的进程：后台跑 IMTest，前台 sample
        "$IMTEST" --gtest_filter="PerfTest.*" &
        TEST_PID=$!
        sleep 1  # 等进程启动
        echo "=== 开始采样 PID=${TEST_PID} ==="
        # sample <pid> <duration_seconds> [-file <output>]
        sample "$TEST_PID" 30 -file "${OUTPUT_DIR}/sample.txt" &
        SAMPLE_PID=$!
        # 等待测试跑完
        wait $TEST_PID
        # sample 到时间会自动退出；若测试先结束则手动终止 sample
        kill $SAMPLE_PID 2>/dev/null || true
        wait $SAMPLE_PID 2>/dev/null || true
        ;;
    none)
        "$IMTEST" --gtest_filter="PerfTest.*"
        ;;
esac

# ── 分析提示 ─────────────────────────────────────────────────────────────────
echo ""
echo "=== 如需分析结果 ==="
echo "  Linux perf:      perf report -i ${OUTPUT_DIR}/perf.data"
echo "  Linux heaptrack: heaptrack_gui ${OUTPUT_DIR}/heaptrack.IMTest.*.gz"
echo "  macOS sample:    open ${OUTPUT_DIR}/sample.txt  # 或用 cmp_sample 对比"

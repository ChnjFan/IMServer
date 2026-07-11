#!/bin/bash
# 性能基线采集 + Profiler 运行脚本
#
# 用法：
#   ./scripts/run_perf.sh                # 仅跑性能测试
#   ./scripts/run_perf.sh --perf         # 同时用 perf record 采样（Linux）
#   ./scripts/run_perf.sh --heaptrack    # 同时用 heaptrack 采样堆内存（Linux）
#
# 输出：
#   cmake-build-debug/bin/perf_report.csv  — 阶梯施压数据
#   <output_dir>/perf.data                  — perf 采样结果（--perf）
#   <output_dir>/heaptrack.IMTest.*.gz      — heaptrack 结果（--heaptrack）

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

IMTEST="./cmake-build-debug/bin/IMTest"
OUTPUT_DIR="./cmake-build-debug/bin"
ARGS=""

# 解析参数
for arg in "$@"; do
    case "$arg" in
        --perf)
            ARGS="perf record -g -o ${OUTPUT_DIR}/perf.data"
            echo "=== 将使用 perf record 采样 CPU ==="
            ;;
        --heaptrack)
            ARGS="heaptrack -o ${OUTPUT_DIR}/heaptrack"
            echo "=== 将使用 heaptrack 采样堆内存 ==="
            ;;
        *)
            echo "未知参数: $arg"
            echo "用法: $0 [--perf|--heaptrack]"
            exit 1
            ;;
    esac
done

# 确保 libssl 能找到
export DYLD_LIBRARY_PATH="/usr/local/mysql-connector-c++-9.7.0/lib64:/opt/homebrew/lib:${DYLD_LIBRARY_PATH:-}"

echo "=== 运行性能测试套件 ==="
$ARGS "$IMTEST" --gtest_filter="PerfTest.*"

echo ""
echo "=== 如需分析结果 ==="
echo "  perf:      perf report -i ${OUTPUT_DIR}/perf.data"
echo "  heaptrack: heaptrack_gui ${OUTPUT_DIR}/heaptrack.IMTest.*.gz"

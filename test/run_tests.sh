#!/bin/bash
# 分层测试运行脚本 — run tests by tag/layer
# Usage:
#   ./run_tests.sh              — 运行全部测试 (默认)
#   ./run_tests.sh unit         — 仅 unit 测试 (快速, 无外部依赖)
#   ./run_tests.sh integration  — 集成测试 (需要 Redis/MySQL)
#   ./run_tests.sh stress       — 压力测试 (需要全部服务 + 内核参数调优)
#   ./run_tests.sh --check      — 仅运行环境检查, 不执行测试
#
# 可选环境变量:
#   SKIP_CHECK=1  — 跳过运行前环境检查
#
# IMTest binary reads gtest filter from --gtest_filter.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
MYSQL_LIB="/usr/local/mysql-connector-c++-9.7.0/lib64"

# 解析参数
TAG="${1:-all}"
CHECK_ONLY=0

if [ "$TAG" = "--check" ]; then
    CHECK_ONLY=1
    TAG="${2:-all}"
fi

# 运行前环境检查 (可通过 SKIP_CHECK=1 跳过)
if [ "${SKIP_CHECK:-0}" != "1" ]; then
    bash "${SCRIPT_DIR}/scripts/check_system.sh" "$TAG" || {
        echo ""
        echo "环境检查未通过。设置 SKIP_CHECK=1 可跳过此检查。"
        exit 1
    }
else
    echo "=== 跳过环境检查 (SKIP_CHECK=1) ==="
fi

if [ "$CHECK_ONLY" -eq 1 ]; then
    exit 0
fi

# Resolve mysql-connector-c++ lib path so libssl/libcrypto are found at runtime
if [ -n "${DYLD_LIBRARY_PATH:-}" ]; then
    export DYLD_LIBRARY_PATH="${MYSQL_LIB}:/usr/local/lib:${DYLD_LIBRARY_PATH}"
else
    export DYLD_LIBRARY_PATH="${MYSQL_LIB}:/usr/local/lib"
fi

IMTEST="${PROJECT_ROOT}/cmake-build-debug/bin/IMTest"

case "$TAG" in
    unit)
        # Exclude integration, stability, perf (they need external deps)
        FILTER="*:-*Integration*"
        ;;
    integration)
        FILTER="*[Ii]ntegration*"
        ;;
    stress)
        # Stress tests only
        FILTER="BurstConnectTest.*:RampUpTest.*:SustainedLoadTest.*:MixedScenarioTest.*"
        ;;
    all)
        FILTER="*"
        ;;
    *)
        echo "Unknown tag: $TAG"
        echo "Usage: $0 [--check] {unit|integration|stress|all}"
        exit 1
        ;;
esac

echo ""
echo "=== Running tests with filter: $FILTER ==="
"$IMTEST" --gtest_filter="$FILTER" --gtest_color=yes

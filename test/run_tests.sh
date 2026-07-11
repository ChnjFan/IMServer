#!/bin/bash
# 分层测试运行脚本 — run tests by tag/layer
# Usage:
#   ./run_tests.sh unit          — unit tests only (fast, no deps)
#   ./run_tests.sh integration   — integration tests (needs Redis/MySQL)
#   ./run_tests.sh all           — everything (default)
#
# IMTest binary reads gtest filter from --gtest_filter.

set -e

# Resolve mysql-connector-c++ lib path so libssl/libcrypto are found at runtime
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
MYSQL_LIB="/usr/local/mysql-connector-c++-9.7.0/lib64"

if [ -n "${DYLD_LIBRARY_PATH:-}" ]; then
    export DYLD_LIBRARY_PATH="${MYSQL_LIB}:${DYLD_LIBRARY_PATH}"
else
    export DYLD_LIBRARY_PATH="${MYSQL_LIB}"
fi

IMTEST="${PROJECT_ROOT}/cmake-build-debug/bin/IMTest"
TAG="${1:-all}"

case "$TAG" in
    unit)
        # Exclude integration, stability, perf (they need external deps)
        FILTER="*:-*Integration*"
        ;;
    integration)
        FILTER="*[Ii]ntegration*"
        ;;
    all)
        FILTER="*"
        ;;
    *)
        echo "Unknown tag: $TAG"
        echo "Usage: $0 {unit|integration|all}"
        exit 1
        ;;
esac

echo "=== Running tests with filter: $FILTER ==="
"$IMTEST" --gtest_filter="$FILTER" --gtest_color=yes

#!/bin/bash
# =============================================================================
# IMServer 压力测试环境检查脚本
#
# 已迁移至 test/scripts/check_system.sh (支持 unit/integration/stress 模式)
# 此文件保留为向后兼容的薄包装器。
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHARE_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")/scripts"

exec bash "${SHARE_DIR}/check_system.sh" stress "$@"

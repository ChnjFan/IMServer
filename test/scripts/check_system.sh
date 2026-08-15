#!/bin/bash
# =============================================================================
# IMServer 测试环境检查脚本
#
# 用途: 运行测试前检查系统环境是否满足要求
# 用法:
#   bash test/scripts/check_system.sh              — 全量检查 (默认)
#   bash test/scripts/check_system.sh unit         — 仅 unit 测试检查
#   bash test/scripts/check_system.sh integration  — 集成测试检查
#   bash test/scripts/check_system.sh stress       — 压力测试检查 (全量)
#
# 检查项:
#   1. 文件描述符限制 (ulimit -n)
#   2. TCP 端口范围与内核参数
#   3. CPU / 内存资源
#   4. 依赖服务 (Redis / MySQL / GateServer / ChatServer / StatusServer)
#   5. 测试二进制文件
#   6. 运行时库路径
# =============================================================================

# 不使用 set -e: 内部通过 ((...)) 做算术时, 值为 0 会返回 exit 1
# 所有错误由显式逻辑处理
set -u

PASS=0
WARN=0
FAIL=0

inc_pass()  { PASS=$((PASS + 1)); }
inc_warn()  { WARN=$((WARN + 1)); }
inc_fail()  { FAIL=$((FAIL + 1)); }

check() {
    local name="$1" actual="$2" threshold="$3" action="$4"
    if [ "$actual" -ge "$threshold" ] 2>/dev/null; then
        echo "  [PASS] $name: $actual (≥$threshold)"
        inc_pass
    else
        echo "  [FAIL] $name: $actual (需要 ≥$threshold)"
        echo "         修复: $action"
        inc_fail
    fi
}

check_warn() {
    local name="$1" actual="$2" threshold="$3" action="$4"
    if [ "$actual" -ge "$threshold" ] 2>/dev/null; then
        echo "  [PASS] $name: $actual (≥$threshold)"
        inc_pass
    else
        echo "  [WARN] $name: $actual (建议 ≥$threshold)"
        echo "         修复: $action"
        inc_warn
    fi
}

check_cmd() {
    local name="$1" cmd="$2"
    if command -v "$cmd" &>/dev/null; then
        local ver
        ver=$(${cmd} --version 2>&1 | head -1) || ver="(version unknown)"
        echo "  [PASS] $name 已安装 ($ver)"
        inc_pass
    else
        echo "  [WARN] $name 未安装 ($cmd)"
        inc_warn
    fi
}

# --- 解析参数 ---
MODE="${1:-all}"

case "$MODE" in
    unit)         DESC="Unit 测试"; NEED_SERVICES=0 ;;
    integration)  DESC="集成测试"; NEED_SERVICES=1 ;;
    stress|all)   DESC="压力测试"; NEED_SERVICES=1 ;;
    *)
        echo "用法: $0 {unit|integration|stress|all}"
        exit 1 ;;
esac

# --- 路径推导 ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
IMTEST="${PROJECT_ROOT}/cmake-build-debug/bin/IMTest"
MYSQL_LIB="/usr/local/mysql-connector-c++-9.7.0/lib64"

echo ""
echo "================================================================="
echo "  IMServer 环境检查 — ${DESC}"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================="
echo ""

# ====== 1. 工具链 ======
echo "--- 工具链 ---"

check_cmd "CMake" cmake
check_cmd "Ninja" ninja
check_cmd "redis-cli" redis-cli

echo ""

# ====== 2. 系统资源 (stress/all 模式才严格检查) ======
if [ "$NEED_SERVICES" -eq 1 ]; then
    echo "--- 系统资源 ---"

    # 文件描述符
    FD_LIMIT=$(ulimit -n)
    if [ "$MODE" = "stress" ] || [ "$MODE" = "all" ]; then
        check "文件描述符 (ulimit -n)" "$FD_LIMIT" 200000 "ulimit -n 200000"
    else
        check_warn "文件描述符 (ulimit -n)" "$FD_LIMIT" 4096 "ulimit -n 4096"
    fi

    # 临时端口范围
    PORT_RANGE=$(sysctl -n net.ipv4.ip_local_port_range 2>/dev/null || echo "32768 60999")
    PORT_MIN=$(echo "$PORT_RANGE" | awk '{print $1}')
    PORT_MAX=$(echo "$PORT_RANGE" | awk '{print $2}')
    PORT_COUNT=$((PORT_MAX - PORT_MIN))
    if [ "$MODE" = "stress" ] || [ "$MODE" = "all" ]; then
        check "临时端口范围" "$PORT_COUNT" 60000 'sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"'
    else
        check_warn "临时端口范围" "$PORT_COUNT" 20000 'sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"'
    fi

    # TCP 内核参数
    if [ "$MODE" = "stress" ] || [ "$MODE" = "all" ]; then
        echo ""
        echo "--- TCP 内核参数 ---"

        TW_REUSE=$(sysctl -n net.ipv4.tcp_tw_reuse 2>/dev/null || echo 0)
        check "tcp_tw_reuse" "$TW_REUSE" 1 'sudo sysctl -w net.ipv4.tcp_tw_reuse=1'

        SYN_BACKLOG=$(sysctl -n net.ipv4.tcp_max_syn_backlog 2>/dev/null || echo 128)
        check_warn "tcp_max_syn_backlog" "$SYN_BACKLOG" 65535 'sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535'

        SOMAXCONN=$(sysctl -n net.core.somaxconn 2>/dev/null || echo 128)
        check_warn "somaxconn" "$SOMAXCONN" 65535 'sudo sysctl -w net.core.somaxconn=65535'
    fi

    # CPU / 内存
    echo ""
    echo "--- 硬件资源 ---"

    CORES=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    check_warn "CPU 核心数" "$CORES" 4 "建议 ≥ 4 cores"

    MEM_BYTES=$(sysctl -n hw.memsize 2>/dev/null || echo 0)
    MEM_GB=$((MEM_BYTES / 1073741824))
    if [ "$MODE" = "stress" ] || [ "$MODE" = "all" ]; then
        check "内存容量" "$MEM_GB" 8 "建议 ≥ 16GB for 10K 连接"
    else
        check_warn "内存容量" "$MEM_GB" 4 "建议 ≥ 8GB"
    fi

    echo ""
fi

# ====== 3. 依赖服务 ======
if [ "$NEED_SERVICES" -eq 1 ]; then
    echo "--- 依赖服务 ---"

    # Redis
    if command -v redis-cli &>/dev/null; then
        if redis-cli -h 127.0.0.1 -p 6379 ping 2>/dev/null | grep -q "PONG"; then
            echo "  [PASS] Redis 连接正常 (127.0.0.1:6379)"
            inc_pass
        else
            echo "  [FAIL] Redis 未启动 (127.0.0.1:6379)"
            echo "         修复: ./bin/redis-server ./config/redis.conf"
            inc_fail
        fi
    else
        echo "  [WARN] redis-cli 未安装，跳过 Redis 检查"
        inc_warn
    fi

    # MySQL
    if command -v mysql &>/dev/null; then
        if mysql -h 127.0.0.1 -P 3306 -u admin -p123456 -e "SELECT 1" IMServerTest &>/dev/null; then
            echo "  [PASS] MySQL 连接正常 (127.0.0.1:3306/IMServerTest)"
            inc_pass
        else
            echo "  [FAIL] MySQL 连接失败 (127.0.0.1:3306/IMServerTest)"
            echo "         修复: 启动 MySQL 并创建 IMServerTest 数据库"
            inc_fail
        fi
    else
        echo "  [WARN] mysql 客户端未安装，跳过 MySQL 检查"
        inc_warn
    fi

    # VerifyServer (Node.js gRPC)
    if command -v nc &>/dev/null; then
        if nc -z -w 2 127.0.0.1 50051 2>/dev/null; then
            echo "  [PASS] VerifyServer gRPC 端口可达 (127.0.0.1:50051)"
            inc_pass
        else
            echo "  [WARN] VerifyServer gRPC 端口不可达 (127.0.0.1:50051)"
            echo "         修复: cd src/VerifyServer && npm start"
            inc_warn
        fi
    else
        echo "  [WARN] nc 未安装，跳过端口检查"
        inc_warn
    fi

    # StatusServer
    if command -v nc &>/dev/null; then
        if nc -z -w 2 127.0.0.1 50052 2>/dev/null; then
            echo "  [PASS] StatusServer gRPC 端口可达 (127.0.0.1:50052)"
            inc_pass
        else
            echo "  [FAIL] StatusServer gRPC 端口不可达 (127.0.0.1:50052)"
            echo "         修复: ./cmake-build-debug/bin/StatusServer"
            inc_fail
        fi
    fi

    # ChatServer
    if command -v nc &>/dev/null; then
        if nc -z -w 2 127.0.0.1 50053 2>/dev/null; then
            echo "  [PASS] ChatServer TCP 端口可达 (127.0.0.1:50053)"
            inc_pass
        else
            echo "  [FAIL] ChatServer TCP 端口不可达 (127.0.0.1:50053)"
            echo "         修复: ./cmake-build-debug/bin/ChatServer"
            inc_fail
        fi
    fi

    # GateServer
    if command -v curl &>/dev/null; then
        HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8080/ 2>/dev/null || echo "000")
        if echo "$HTTP_CODE" | grep -qE "^(200|404|405|501)$"; then
            echo "  [PASS] GateServer HTTP 端口可达 (127.0.0.1:8080, status=$HTTP_CODE)"
            inc_pass
        else
            echo "  [FAIL] GateServer HTTP 端口不可达 (127.0.0.1:8080)"
            echo "         修复: ./cmake-build-debug/bin/GateServer"
            inc_fail
        fi
    else
        echo "  [WARN] curl 未安装，跳过 GateServer 检查"
        inc_warn
    fi

    echo ""
fi

# ====== 4. 测试二进制 ======
echo "--- 测试二进制 ---"

if [ -x "$IMTEST" ]; then
    IMTEST_SIZE=$(du -h "$IMTEST" | cut -f1)
    echo "  [PASS] IMTest 可执行文件存在 (${IMTEST_SIZE})"
    inc_pass
else
    echo "  [FAIL] IMTest 可执行文件不存在或不可执行"
    echo "         修复: cd cmake-build-debug && cmake --build . --target IMTest"
    inc_fail
fi

# 运行时库
if [ -d "$MYSQL_LIB" ]; then
    echo "  [PASS] MySQL Connector 库目录存在 (${MYSQL_LIB})"
    inc_pass
else
    echo "  [WARN] MySQL Connector 库目录不存在: ${MYSQL_LIB}"
    inc_warn
fi

# ====== 总结 ======
echo ""
echo "================================================================="
echo "  检查完成: $PASS 通过 / $WARN 警告 / $FAIL 失败"
echo "================================================================="

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "❌ 有 $FAIL 项检查失败，请先修复后再运行 ${DESC}。"
    echo ""

    if [ "$NEED_SERVICES" -eq 1 ]; then
        echo "快速修复命令 (需要 sudo):"
        echo "  ulimit -n 200000"
        echo "  sudo sysctl -w net.ipv4.ip_local_port_range=\"1024 65535\""
        echo "  sudo sysctl -w net.ipv4.tcp_tw_reuse=1"
        echo "  sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535"
        echo "  sudo sysctl -w net.core.somaxconn=65535"
        echo ""
        echo "启动服务 (按顺序):"
        echo "  ./bin/redis-server ./config/redis.conf"
        echo "  cd src/VerifyServer && npm start &"
        echo "  ./cmake-build-debug/bin/StatusServer &"
        echo "  ./cmake-build-debug/bin/ChatServer &"
        echo "  ./cmake-build-debug/bin/GateServer &"
    fi
    exit 1
fi

if [ "$WARN" -gt 0 ]; then
    echo ""
    echo "⚠️  有 $WARN 项警告，${DESC} 可能受限。"
fi

echo ""
echo "✅ 系统环境满足 ${DESC}要求！"
echo ""

if [ "$NEED_SERVICES" -eq 1 ]; then
    echo "运行测试:"
    echo "  export DYLD_LIBRARY_PATH=${MYSQL_LIB}:/usr/local/lib:\$DYLD_LIBRARY_PATH"
    case "$MODE" in
        integration)  FILTER='*[Ii]ntegration*' ;;
        stress|all)   FILTER='BurstConnectTest.*:RampUpTest.*:SustainedLoadTest.*:MixedScenarioTest.*' ;;
        *)            FILTER='*' ;;
    esac
    echo "  ./cmake-build-debug/bin/IMTest --gtest_filter=\"${FILTER}\""
else
    echo "运行测试:"
    echo "  ./cmake-build-debug/bin/IMTest"
fi

echo ""
exit 0

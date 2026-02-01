#!/bin/bash

# 生成gRPC代码的shell脚本

# 设置变量
GEN_DIR="generated"
PROTOC="protoc"
GRPC_PLUGIN="grpc_cpp_plugin"

# 检查protoc是否存在
if ! command -v "$PROTOC" &> /dev/null; then
    # 尝试常见的安装路径
    if [ -f "/usr/local/bin/protoc" ]; then
        PROTOC="/usr/local/bin/protoc"
        echo "Found protoc at /usr/local/bin/protoc"
    elif [ -f "/usr/bin/protoc" ]; then
        PROTOC="/usr/bin/protoc"
        echo "Found protoc at /usr/bin/protoc"
    else
        echo "Error: protoc not found in PATH"
        echo "Please install Protocol Buffers and add to PATH"
        echo "Or install to /usr/local/bin/protoc"
        exit 1
    fi
fi

# 检查grpc_cpp_plugin是否存在
if ! command -v "$GRPC_PLUGIN" &> /dev/null; then
    # 尝试常见的安装路径
    if [ -f "/usr/local/bin/grpc_cpp_plugin" ]; then
        GRPC_PLUGIN="/usr/local/bin/grpc_cpp_plugin"
        echo "Found grpc_cpp_plugin at /usr/local/bin/grpc_cpp_plugin"
    elif [ -f "/usr/bin/grpc_cpp_plugin" ]; then
        GRPC_PLUGIN="/usr/bin/grpc_cpp_plugin"
        echo "Found grpc_cpp_plugin at /usr/bin/grpc_cpp_plugin"
    else
        echo "Error: grpc_cpp_plugin not found in PATH"
        echo "Please install gRPC and add to PATH"
        echo "Or install to /usr/local/bin/grpc_cpp_plugin"
        exit 1
    fi
fi

# 创建生成目录
if [ ! -d "$GEN_DIR" ]; then
    mkdir -p "$GEN_DIR"
    echo "Created directory: $GEN_DIR"
fi

# 查找当前目录下所有的proto文件
PROTO_FILES=$(find . -name "*.proto" -type f | grep -v "$GEN_DIR")

if [ -z "$PROTO_FILES" ]; then
    echo "Error: No .proto files found in current directory"
    exit 1
fi

echo "Found proto files:"
echo "$PROTO_FILES"

# 生成protobuf代码
echo "\nGenerating protobuf code..."
for proto_file in $PROTO_FILES; do
    echo "Processing $proto_file..."
    $PROTOC -I=. --cpp_out="$GEN_DIR" "$proto_file"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate protobuf code for $proto_file"
        exit 1
    fi
done

# 生成gRPC代码
echo "\nGenerating gRPC code..."
for proto_file in $PROTO_FILES; do
    echo "Processing $proto_file..."
    $PROTOC -I=. --grpc_out="$GEN_DIR" --plugin=protoc-gen-grpc="$GRPC_PLUGIN" "$proto_file"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate gRPC code for $proto_file"
        exit 1
    fi
done

echo "\nSuccess: Generated gRPC code in $GEN_DIR"
ls -la "$GEN_DIR"

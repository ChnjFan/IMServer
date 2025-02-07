#!/bin/sh
SRC_DIR=./protocol
DST_DIR=./protocol

../../third_party/grpc/bin/protoc -I=$SRC_DIR --grpc_out=$DST_DIR --plugin=protoc-gen-grpc=../../third_party/grpc/bin/grpc_cpp_plugin --experimental_allow_proto3_optional $SRC_DIR/*.proto
../../third_party/grpc/bin/protoc -I=$SRC_DIR --cpp_out=$DST_DIR $SRC_DIR/*.proto


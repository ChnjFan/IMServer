#!/bin/sh
SRC_DIR=./protocol
DST_DIR=./protocol

../../third_party/protobuf/bin/protoc -I=$SRC_DIR --cpp_out=$DST_DIR $SRC_DIR/*.proto
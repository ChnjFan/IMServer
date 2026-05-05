const path = require('path')
const grpc = require('@grpc/grpc-js')
const protoLoader = require('@grpc/proto-loader')

const PROTO_DIR = path.join(__dirname, '../../proto')
const PROTO_PATH = path.join(PROTO_DIR, 'message.proto')

const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
    keepCase: true, // 保持原始大小写
    longs: String,  // 长整形转换为字符串
    enums: String,  // 枚举转换为字符串
    defaults: true, // 保持原始默认值定义
    oneofs: true,   // 保持原始oneof定义
})

const protoDescriptor = grpc.loadPackageDefinition(packageDefinition)

const message_proto = protoDescriptor.message

// 导出message_proto
module.exports = message_proto
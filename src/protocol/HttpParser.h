#pragma once

#include "Parser.h"

namespace protocol {

// HTTP解析状态枚举
enum class HttpParseState {
    Initial,              // 初始状态
    Headers,              // 解析头字段
    Body,                 // 解析消息体
    ChunkedBodyStart,     // 解析分块传输的块大小
    ChunkedBodyData,      // 解析分块数据
    ChunkedBodyEnd,       // 解析分块结束
    Complete              // 解析完成
};

/**
 * @brief HTTP协议解析器
 * 解析HTTP请求，生成统一的Message对象
 */
class HttpParser : public Parser {
private:
    bool is_parsing_;                   // 是否正在解析
    HttpMessage parsed_message_;        // 当前解析的消息
    std::mutex mutex_;                  // 互斥锁，保护解析状态

public:
    HttpParser();
    ~HttpParser() override = default;

    void reset() override;
    
    /**
     * @brief 异步解析HTTP数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    void asyncParse(
        const std::vector<char>& data,
        ParseCallback callback) override;
    
    /**
     * @brief 获取协议类型
     * @return network::ConnectionType::HTTP
     */
    network::ConnectionType getType() const override {
        return network::ConnectionType::HTTP;
    }
};

} // namespace protocol

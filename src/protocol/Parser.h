#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <boost/system/error_code.hpp>

#include "ConnectionManager.h"
#include "Message.h"

namespace protocol {

/**
 * @brief 协议解析器抽象基类
 * 负责解析不同协议格式的数据，生成统一的Message对象
 */
class Parser {
protected:
    network::ConnectionId connection_id_; // 关联的连接ID

public:
    using Ptr = std::shared_ptr<Parser>;
    using ParseCallback = std::function<void(const boost::system::error_code&, Message&&)>;

    virtual ~Parser() = default;

    /**
     * @brief 异步解析数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    virtual void asyncParse(const std::vector<char>& data, ParseCallback callback) = 0;

    /**
     * @brief 获取协议类型
     * @return network::ConnectionType 协议类型
     */
    virtual network::ConnectionType getType() const = 0;

    /**
     * @brief 重置解析器状态
     */
    virtual void reset() = 0;

    /**
     * @brief 设置解析器的连接ID
     * @param connection_id 连接ID
     */
    void setConnectionId(network::ConnectionId connection_id) {
        connection_id_ = connection_id;
    }

    /**
     * @brief 获取解析器的连接ID
     * @return network::ConnectionId 连接ID
     */
    network::ConnectionId getConnectionId() const {
        return connection_id_;
    }
};

/**
 * @brief TCP协议解析器
 * 解析固定长度消息头+可变长度消息体的TCP协议格式
 */
class TcpParser : public Parser {
private:
    /**
     * @brief 解析状态枚举
     */
    enum class ParseState {
        Header,  // 解析消息头
        Body     // 解析消息体
    };

    // 状态机相关
    ParseState state_;                 // 当前解析状态
    MessageHeader header_;             // 当前解析的消息头
    std::vector<char> body_buffer_;    // 消息体缓冲区
    size_t expected_body_length_;      // 预期的消息体长度
    std::mutex mutex_;                 // 互斥锁，保护解析状态
public:
    TcpParser();
    ~TcpParser() override = default;

    /**
     * @brief 异步解析TCP数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    void asyncParse(const std::vector<char>& data, ParseCallback callback) override;

    /**
     * @brief 获取协议类型
     * @return network::ConnectionType::TCP
     */
    network::ConnectionType getType() const override {
        return network::ConnectionType::TCP;
    }

    /**
     * @brief 重置解析器状态
     */
    void reset() override;
    
private:    
    /**
     * @brief 解析消息头
     * @param data 待解析的数据
     * @param consumed 已消费的数据长度
     * @return bool 解析是否成功
     */
    bool parseHeader(const std::vector<char>& data, size_t& consumed);
    
    /**
     * @brief 解析消息体
     * @param data 待解析的数据
     * @param consumed 已消费的数据长度
     * @return bool 解析是否成功
     */
    bool parseBody(const std::vector<char>& data, size_t& consumed);
};

/**
 * @brief WebSocket协议解析器
 * 解析WebSocket帧，提取实际消息内容
 */
class WebSocketParser : public Parser {
public:
    /**
     * @brief 构造函数
     */
    WebSocketParser();
    
    /**
     * @brief 析构函数
     */
    ~WebSocketParser() override = default;
    
    /**
     * @brief 异步解析WebSocket数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    void asyncParse(
        const std::vector<char>& data,
        ParseCallback callback) override;
    
    /**
     * @brief 获取协议类型
     * @return network::ConnectionType::WebSocket
     */
    network::ConnectionType getType() const override {
        return network::ConnectionType::WebSocket;
    }
    
    /**
     * @brief 重置解析器状态
     */
    void reset() override;
    
private:
    /**
     * @brief WebSocket帧解析状态枚举
     */
    enum class FrameState {
        Initial,       // 初始状态
        Header,        // 解析帧头
        PayloadLength, // 解析载荷长度
        ExtendedLength,// 解析扩展长度
        MaskingKey,    // 解析掩码密钥
        Payload,       // 解析载荷
        Complete       // 解析完成
    };
    
    // WebSocket帧相关
    FrameState state_;                 // 当前解析状态
    std::vector<char> current_frame_;  // 当前帧数据
    std::vector<char> message_buffer_; // 完整消息缓冲区
    bool is_final_frame_;              // 是否为消息的最后一帧
    uint8_t opcode_;                   // 当前帧操作码
    uint64_t payload_length_;          // 载荷长度
    std::vector<char> masking_key_;    // 掩码密钥
    std::mutex mutex_;                 // 互斥锁，保护解析状态
};

/**
 * @brief HTTP协议解析器
 * 解析HTTP请求，生成统一的Message对象
 */
class HttpParser : public Parser {
public:
    /**
     * @brief 构造函数
     */
    HttpParser();
    
    /**
     * @brief 析构函数
     */
    ~HttpParser() override = default;
    
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
    
    /**
     * @brief 重置解析器状态
     */
    void reset() override;
    
private:
    std::vector<char> buffer_;          // 解析缓冲区
    std::mutex mutex_;                 // 互斥锁，保护解析状态
};

/**
 * @brief 解析器工厂类
 * 负责创建和管理不同类型的协议解析器
 */
class ParserFactory {
public:
    /**
     * @brief 获取ParserFactory的单例实例
     * @return ParserFactory& 单例引用
     */
    static ParserFactory& instance();
    
    /**
     * @brief 创建协议解析器
     * @param connection_type 连接类型
     * @return std::shared_ptr<Parser> 解析器指针
     */
    std::shared_ptr<Parser> createParser(network::ConnectionType connection_type);
    
    /**
     * @brief 注册自定义解析器
     * @param connection_type 连接类型
     * @param creator 解析器创建函数
     */
    void registerParser(
        network::ConnectionType connection_type,
        std::function<std::shared_ptr<Parser>()> creator);
    
private:
    /**
     * @brief 私有构造函数，防止外部实例化
     */
    ParserFactory();
    
    // 解析器创建函数映射
    std::unordered_map<
        network::ConnectionType,
        std::function<std::shared_ptr<Parser>()>
    > parser_creators_;
    
    // 解析器实例缓存
    std::unordered_map<
        network::ConnectionType,
        std::weak_ptr<Parser>
    > parser_instances_;
    
    std::mutex mutex_; // 互斥锁，保护解析器映射和缓存
};

} // namespace protocol

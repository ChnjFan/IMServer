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

//
// Created by Fan on 2026/07/25.
//

#ifndef IMSERVER_CHATMSG_NODE_H
#define IMSERVER_CHATMSG_NODE_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "common/model/MessageInfo.h"

class Session;

/**
 * @brief 批量写入队列中的消息节点。
 *
 * 由 IO 线程分配 (new), DB 线程消费后释放 (delete)。
 * 使用 weak_ptr<Session> 避免循环引用：Session 的生命周期由
 * ChatServer 管理，如果客户端断连，weak_ptr 自动失效。
 */
struct ChatMsgNode {
    MessageInfo              msg;
    std::weak_ptr<Session>   sender_session;
    int64_t                  enqueue_time_us;  ///< 入队时间戳 (steady_clock μs)

    ChatMsgNode() = default;

    ChatMsgNode(const MessageInfo& m, const std::shared_ptr<Session>& s)
        : msg(m)
        , sender_session(s)
        , enqueue_time_us(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count())
    {}
};

#endif //IMSERVER_CHATMSG_NODE_H

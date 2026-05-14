//
// Created by Fan on 2026/5/11.
//

#ifndef IMSERVER_SESSION_H
#define IMSERVER_SESSION_H

#include <queue>

#include "const.h"
#include "MsgNode.h"

class ChatServer;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(net::io_context &io_context, const std::shared_ptr<ChatServer> &chatServer);
    ~Session();

    void start();
    void close();

    tcp::socket & getSocket();
    std::string getSessionId();

    void asyncSend(const std::string &msg, std::uint16_t msgId);
    void asyncSend(const char* msg, std::uint16_t size, std::uint16_t msgId);

private:
    void asyncReadHead(std::uint16_t totalLen);
    void asyncReadBody(std::uint16_t size);
    void asyncReadFull(std::uint16_t totalLen,
        const std::function<void(const boost::system::error_code&, std::uint16_t)>& callback);
    void asyncReadSome(std::uint16_t readLen, std::uint16_t totalLen,
        const std::function<void(const boost::system::error_code &, std::uint16_t)>& callback);

    void asyncSend();

    static constexpr int MAX_SEND_QUEUE = 1024;

    std::atomic<bool> stop_;
    std::string sessionId_;
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    std::shared_ptr<ChatServer> chatServer_;

    std::shared_ptr<MsgNode> headNode_;
    std::shared_ptr<RecvNode> recvNode_;
    std::queue<std::shared_ptr<SendNode>> sendNodeQueue_;   // 同一个会话异步回复多个消息
    std::mutex sendMtx_;

    char buffer_[MAX_BUFFER_SIZE];
};


#endif //IMSERVER_SESSION_H
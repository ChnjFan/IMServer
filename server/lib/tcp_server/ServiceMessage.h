//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVICEMESSAGE_H
#define IMSERVER_SERVICEMESSAGE_H

#include <unordered_map>
#include "Message.h"
#include "BlockingQueue.h"
#include "Poco/Mutex.h"

namespace TcpServerNet {

using ServiceMessageQueue = Base::BlockingQueue<Base::Message>;
using ServiceMessageQueueMap = std::unordered_map<std::string, ServiceMessageQueue>;

class ServiceMessage {
public:
    static ServiceMessage* getInstance();

    // 每个tcp连接都应该有一个发送队列和接收队列
    ServiceMessageQueue& getSendQueue(std::string& connName);
    void pushTaskMessage(Base::Message &message, std::string connName);
    bool tryGetTaskMessage(Base::Message &message, std::string& connName);
    void clear(std::string& connName);

private:
    ServiceMessage() = default;

    ServiceMessageQueueMap send;
    ServiceMessageQueueMap recv;

    static ServiceMessage* serviceMessage;
    static Poco::Mutex mutex;
};

}

#endif //IMSERVER_SERVICEMESSAGE_H

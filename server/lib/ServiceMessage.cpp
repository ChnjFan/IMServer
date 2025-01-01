//
// Created by fan on 24-12-29.
//

#include "ServiceMessage.h"

ServerNet::ServiceMessage* ServerNet::ServiceMessage::serviceMessage = nullptr;
Poco::Mutex ServerNet::ServiceMessage::mutex;

ServerNet::ServiceMessage *ServerNet::ServiceMessage::getInstance() {
    if (serviceMessage == nullptr) {
        Poco::Mutex::ScopedLock lock(mutex);
        if (serviceMessage == nullptr) {
            serviceMessage = new ServiceMessage();
        }
    }
    return serviceMessage;
}

ServerNet::ServiceMessageQueue &ServerNet::ServiceMessage::getSendQueue(std::string& connName) {
    return send[connName];
}

void ServerNet::ServiceMessage::pushTaskMessage(Base::Message &message, std::string connName) {
    Poco::Mutex::ScopedLock lock(mutex);
    recv[connName].push(message);
}

bool ServerNet::ServiceMessage::tryGetTaskMessage(Base::Message &message, std::string &connName) {
    Poco::Mutex::ScopedLock lock(mutex);
    for (auto & it : recv) {
        if (it.second.tryPop(message)) {
            connName = it.first;
            return true;
        }
    }
    return false;
}

void ServerNet::ServiceMessage::clear(std::string &connName) {
    Poco::Mutex::ScopedLock lock(mutex);
    send.erase(connName);
    recv.erase(connName);
}

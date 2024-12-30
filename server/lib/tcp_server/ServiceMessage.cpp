//
// Created by fan on 24-12-29.
//

#include "ServiceMessage.h"

TcpServerNet::ServiceMessage* serviceMessage = nullptr;
Poco::Mutex TcpServerNet::ServiceMessage::mutex;

TcpServerNet::ServiceMessage *TcpServerNet::ServiceMessage::getInstance() {
    if (serviceMessage == nullptr) {
        Poco::Mutex::ScopedLock lock(mutex);
        if (serviceMessage == nullptr) {
            serviceMessage = new ServiceMessage();
        }
    }
    return serviceMessage;
}

TcpServerNet::ServiceMessageQueue &TcpServerNet::ServiceMessage::getSendQueue(std::string& connName) {
    return send[connName];
}

void TcpServerNet::ServiceMessage::pushTaskMessage(Base::Message &message, std::string connName) {
    Poco::Mutex::ScopedLock lock(mutex);
    recv[connName].push(message);
}

bool TcpServerNet::ServiceMessage::tryGetTaskMessage(Base::Message &message, std::string &connName) {
    Poco::Mutex::ScopedLock lock(mutex);
    for (auto & it : recv) {
        if (it.second.tryPop(message)) {
            connName = it.first;
            return true;
        }
    }
    return false;
}

void TcpServerNet::ServiceMessage::clear(std::string &connName) {
    Poco::Mutex::ScopedLock lock(mutex);
    send.erase(connName);
    recv.erase(connName);
}

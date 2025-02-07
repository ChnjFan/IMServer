//
// Created by fan on 24-12-29.
//

#include "ServiceMessage.h"

void ServerNet::ServiceMessage::sendServiceResult(Base::Message &message) {
    send.push(message);
}

void ServerNet::ServiceMessage::pushTaskMessage(Base::Message &message) {
    recv.push(message);
}

bool ServerNet::ServiceMessage::tryGetTaskMessage(Base::Message &message) {
    return recv.tryPop(message);
}

bool ServerNet::ServiceMessage::tryGetTaskMessage(Base::Message &message, long milliseconds) {
    return recv.tryPopFor(message, milliseconds);
}

bool ServerNet::ServiceMessage::tryGetResultMessage(Base::Message &message) {
    return send.tryPop(message);
}

bool ServerNet::ServiceMessage::tryGetResultMessage(Base::Message &message, long milliseconds) {
    return send.tryPopFor(message, milliseconds);
}

void ServerNet::ServiceMessage::clear() {
    Poco::Mutex::ScopedLock lock(mutex);
    send.clear();
    recv.clear();
}

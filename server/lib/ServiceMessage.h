//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVICEMESSAGE_H
#define IMSERVER_SERVICEMESSAGE_H

#include <unordered_map>
#include "Message.h"
#include "BlockingQueue.h"
#include "Poco/Mutex.h"

namespace ServerNet {

using ServiceMessageQueue = Base::BlockingQueue<Base::Message>;

class ServiceMessage {
public:
    void sendServiceResult(Base::Message &message);
    void pushTaskMessage(Base::Message &message);
    bool tryGetTaskMessage(Base::Message &message);
    bool tryGetTaskMessage(Base::Message &message, long milliseconds);
    void clear();

    ServiceMessage() = default;

private:

    ServiceMessageQueue send;
    ServiceMessageQueue recv;

    Poco::Mutex mutex;
};

}

#endif //IMSERVER_SERVICEMESSAGE_H

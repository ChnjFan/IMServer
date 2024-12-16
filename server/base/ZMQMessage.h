//
// Created by fan on 24-12-16.
//

#ifndef IMSERVER_ZMQMESSAGE_H
#define IMSERVER_ZMQMESSAGE_H

#include "zmq.hpp"

namespace Base {

class ZMQMessage {
public:
    ZMQMessage() = default;

    ZMQMessage(ZMQMessage &message) {
        identity.copy(message.identity);
    }


    ZMQMessage(ZMQMessage const &message) {
        identity.rebuild(message.identity.data(), message.identity.size());
        msg.rebuild(message.msg.data(), message.msg.size());
    }

    ZMQMessage& operator=(const ZMQMessage& message) {
        identity.rebuild(message.identity.data(), message.identity.size());
        msg.rebuild(message.msg.data(), message.msg.size());
        return *this;
    }

    zmq::message_t &getIdentity() const {
        return (zmq::message_t &) std::move(identity);
    }

    void setIdentity(zmq::message_t &pIdentity) {
        ZMQMessage::identity.copy(pIdentity);
    }

    zmq::message_t &getMsg() const {
        return (zmq::message_t &) std::move(msg);
    }

    void setMsg(zmq::message_t &pMsg) {
        ZMQMessage::msg.copy(pMsg);
    }


private:
    zmq::message_t identity;
    zmq::message_t msg;
};

}

#endif //IMSERVER_ZMQMESSAGE_H

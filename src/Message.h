//
// Created by fan on 25-2-26.
//

#ifndef IMSERVER_MESSAGE_H
#define IMSERVER_MESSAGE_H

#include <utility>

#include "google/protobuf/message.h"

namespace Base {

class Message;
using MessagePtr = std::shared_ptr<Message>;

class Message {
public:
    explicit Message(google::protobuf::Message *pMsg) {
        message.reset(pMsg);
    }

    static MessagePtr getMessage(void *data, int size, const std::string& type) {
        google::protobuf::Message *pMsg = createMessage(type);
        if (pMsg) {
            pMsg->ParseFromArray(data, size);
        }
        MessagePtr result = std::make_shared<Message>(pMsg);
        return result;
    }

private:
    static google::protobuf::Message* createMessage(const std::string& typeName)
    {
        google::protobuf::Message* message = nullptr;
        const google::protobuf::Descriptor* descriptor =
                google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
        if (descriptor)
        {
            const google::protobuf::Message* prototype =
                    google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
            if (prototype)
            {
                message = prototype->New();
            }
        }
        return message;
    }

    std::shared_ptr<google::protobuf::Message> message;
};

}

#endif //IMSERVER_MESSAGE_H

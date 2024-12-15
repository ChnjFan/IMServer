//
// Created by fan on 24-12-11.
//

#include "AccountWorker.h"

AccountWorker::AccountWorker(zmq::context_t &ctx, zmq::socket_type socType) : BaseWorker(ctx, socType) {
    std::cout << "new Account Worker" << std::endl;
}

void AccountWorker::onReadable() {
    Base::BlockingQueue<Base::ZMQMessage>& recvMsgQueue = getRecvMsgQueue();
    Base::BlockingQueue<Base::ZMQMessage>& sendMsgQueue = getSendMsgQueue();
    Base::ZMQMessage message;
    if (!recvMsgQueue.tryPopFor(message, 500))
        return;
    std::cout << "Recv " << message.getIdentity().to_string() << " request: " << message.getMsg().to_string() << std::endl;

    sendMsgQueue.push(message);
}

void AccountWorker::onError() {
    BaseWorker::onError();
}

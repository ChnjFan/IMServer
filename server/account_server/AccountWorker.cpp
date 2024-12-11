//
// Created by fan on 24-12-11.
//

#include "AccountWorker.h"

AccountWorker::AccountWorker(zmq::context_t &ctx, zmq::socket_type socType) : BaseWorker(ctx, socType) {
    std::cout << "new Account Worker" << std::endl;
}

void AccountWorker::onReadable() {
}

void AccountWorker::onError() {
    BaseWorker::onError();
}

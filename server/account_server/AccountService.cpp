//
// Created by fan on 24-12-6.
//

#include "Exception.h"
#include "AccountService.h"

AccountService::AccountService(Base::ServiceParam &param)
                                : BaseService(param)
                                , threadSize(param.getThreadPoolSize())
                                , workers() {
}

void AccountService::start() {
    // 启动 threadSize 个线程，用于接收用户服务请求并处理
    for (uint32_t i = 0; i < threadSize; ++i) {
        workers.push_back(std::make_shared<AccountWorker>(Base::BaseService::context, zmq::socket_type::dealer));
    }

    Base::BaseService::start();
}

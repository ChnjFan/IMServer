//
// Created by fan on 24-12-11.
//

#ifndef IMSERVER_ACCOUNTWORKER_H
#define IMSERVER_ACCOUNTWORKER_H

#include "BaseService.h"

class AccountWorker : public Base::BaseWorker {
public:
    AccountWorker(zmq::context_t &ctx, zmq::socket_type socType);

    void onReadable() override;
    void onError() override;

private:
};


#endif //IMSERVER_ACCOUNTWORKER_H

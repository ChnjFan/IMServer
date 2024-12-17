//
// Created by fan on 24-12-6.
//

#ifndef IMSERVER_ACCOUNTSERVICE_H
#define IMSERVER_ACCOUNTSERVICE_H

#include "ServiceParam.h"
#include "BaseService.h"
#include "IM.AccountServer.pb.h"
#include "AccountWorker.h"

/**
 * @class AccountService
 * @brief 用户相关服务
 */
class AccountService : public Base::BaseService {
public:
    explicit AccountService(Base::ServiceParam& param);

    void start();

private:
    uint32_t threadSize;
    std::vector<std::shared_ptr<AccountWorker>> workers;
};


#endif //IMSERVER_ACCOUNTSERVICE_H

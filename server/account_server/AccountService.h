//
// Created by fan on 24-12-6.
//

#ifndef IMSERVER_ACCOUNTSERVICE_H
#define IMSERVER_ACCOUNTSERVICE_H

#include "ServiceParam.h"
#include "BaseService.h"
#include "IM.AccountServer.pb.h"

/**
 * @class AccountService
 * @brief 用户相关服务
 */
class AccountService : public Base::BaseService {
public:
    using AccountMsgCallback = std::function<void(AccountService& , std::vector<zmq::message_t>&, Base::Message&)>;

    explicit AccountService(Base::ServiceParam& param);

protected:
    void messageProcessor() override;

private:
    void registerCallback(const char* typeName, AccountMsgCallback callback);
    void registerService();
    void invokeService(std::string& typeName, std::vector<zmq::message_t>& part, Base::Message &message);

    static void login(AccountService& service, std::vector<zmq::message_t>& part, Base::Message& message);
    static void registerUser(AccountService& service, std::vector<zmq::message_t>& part, Base::Message& message);

    std::map<std::string, AccountMsgCallback> callbackMap;
};


#endif //IMSERVER_ACCOUNTSERVICE_H

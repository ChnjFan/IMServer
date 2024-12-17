//
// Created by fan on 24-12-11.
//

#ifndef IMSERVER_ACCOUNTWORKER_H
#define IMSERVER_ACCOUNTWORKER_H

#include "BaseService.h"

class AccountWorker : public Base::BaseWorker {
public:
    using AccountMsgCallback = std::function<void(AccountWorker&, Base::ZMQMessage&, Base::Message&)>;

    AccountWorker(zmq::context_t &ctx, zmq::socket_type socType);

    void onReadable() override;
    void onError() override;

private:
    void registerCallback(const char* typeName, AccountMsgCallback callback);
    void registerService();
    void invokeService(std::string& typeName, Base::ZMQMessage& zmqMsg, Base::Message &message);

    static void errRequest(AccountWorker& worker, Base::ZMQMessage& zmqMsg, Base::Message& message);
    static void login(AccountWorker& worker, Base::ZMQMessage& zmqMsg, Base::Message& message);
    static void registerUser(AccountWorker& worker, Base::ZMQMessage& zmqMsg, Base::Message& message);

private:
    std::map<std::string, AccountMsgCallback> callbackMap;

};


#endif //IMSERVER_ACCOUNTWORKER_H

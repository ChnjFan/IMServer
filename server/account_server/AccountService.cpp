//
// Created by fan on 24-12-6.
//

#include "Exception.h"
#include "AccountService.h"
#include "IM.AccountServer.pb.h"

AccountService::AccountService(Base::ServiceParam &param) : BaseService(param), callbackMap() {
    registerService();
}

void AccountService::messageProcessor() {
    try {
        Base::ZMQMessage zmqMsg = getMessageQueue().pop();
        Base::MessagePtr pMessage = Base::Message::getMessage(zmqMsg.getContent());
        if (nullptr == pMessage)
            return;

        invokeService(pMessage->getTypeName(), zmqMsg.getRoutingParts(), *pMessage);
    }
    catch (Base::Exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void AccountService::registerCallback(const char* typeName, AccountService::AccountMsgCallback callback) {
    callbackMap[typeName] = callback;
}

void AccountService::registerService() {
    registerCallback("IM.Account.ImMsgLoginReq", AccountService::login);
}

void AccountService::invokeService(std::string& typeName, std::vector<zmq::message_t>& part, Base::Message &message) {
    auto it = callbackMap.find(typeName);
    if (it == callbackMap.end())
        return;
    it->second(*this, part, message);
}

void AccountService::login(AccountService& service, std::vector<zmq::message_t>& part, Base::Message &message) {
    IM::Account::ImMsgLoginRes response;
    Poco::Timestamp timestamp;
    //TODO: 登录终端uid设置
    response.set_uid("0");
    response.set_server_time(timestamp.epochMicroseconds());
    response.set_ret_code(IM::BaseType::ResultType::RESULT_TYPE_SUCCESS);

    int size = response.ByteSizeLong();
    char *content = new char[size] ;
    response.SerializeToArray(content, size);
    service.sendResponse(part, content, size);
    delete content;
}

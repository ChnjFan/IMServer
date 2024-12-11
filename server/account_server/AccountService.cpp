//
// Created by fan on 24-12-6.
//

#include "Exception.h"
#include "AccountService.h"

#include <utility>
#include "IM.AccountServer.pb.h"

AccountService::AccountService(Base::ServiceParam &param)
                                : BaseService(param)
                                , callbackMap()
                                , worker(Base::BaseService::context, zmq::socket_type::dealer) {
    registerService();
}

void AccountService::registerCallback(const char* typeName, AccountService::AccountMsgCallback callback) {
    callbackMap[typeName] = std::move(callback);
}

void AccountService::registerService() {
    registerCallback("IM.Account.ImMsgLoginReq", AccountService::login);
    registerCallback("IM.Account.ImMsgRegisterReq", AccountService::registerUser);
}

void AccountService::start() {
    Base::BaseService::start(worker);
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
    char *content = new char[size];
    response.SerializeToArray(content, size);
    delete content;
}

void AccountService::registerUser(AccountService &service, std::vector<zmq::message_t> &part, Base::Message &message) {

}

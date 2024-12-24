//
// Created by fan on 24-12-11.
//

#include "AccountWorker.h"
#include "IM.BaseType.pb.h"
#include "IM.AccountServer.pb.h"
#include "Exception.h"
#include "UserInfo.h"

AccountWorker::AccountWorker(zmq::context_t &ctx, zmq::socket_type socType) : BaseWorker(ctx, socType) {
    std::cout << "new Account Worker" << std::endl;
}

void AccountWorker::onReadable() {
    Base::BlockingQueue<Base::ZMQMessage>& recvMsgQueue = getRecvMsgQueue();
    Base::ZMQMessage message;
    if (!recvMsgQueue.tryPopFor(message, 500))
        return;
    std::cout << "Recv " << message.getIdentity().to_string() << " request: " << message.getMsg().to_string() << std::endl;

    Base::ByteStream request(message.getMsg().size());
    request.write(static_cast<char*>(message.getMsg().data()), message.getMsg().size());
    Base::MessagePtr pMessage = Base::Message::getMessage(request);

    invokeService(pMessage->getTypeName(), message, *pMessage);
}

void AccountWorker::onError() {
    BaseWorker::onError();
}

void AccountWorker::registerCallback(const char* typeName, AccountWorker::AccountMsgCallback callback) {
    callbackMap[typeName] = std::move(callback);
}

void AccountWorker::registerService() {
    registerCallback("IM.Account.ImMsgLoginReq", AccountWorker::login);
    registerCallback("IM.Account.ImMsgRegisterReq", AccountWorker::registerUser);
}

void AccountWorker::invokeService(std::string& typeName, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    auto it = callbackMap.find(typeName);
    if (it == callbackMap.end())
        errRequest(*this, zmqMsg, message);

    try {
        it->second(*this, zmqMsg, message);
    }
    catch (Base::Exception& e) {
        std::cerr << e.what() << std::endl;
        errRequest(*this, zmqMsg, message);
    }
}

void AccountWorker::errRequest(AccountWorker &worker, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    IM::BaseType::ImMsgError errMsg;

    errMsg.set_error_type(IM::BaseType::RESULT_TYPE_REQUEST_ERR);
    worker.send(zmqMsg, errMsg, errMsg.ByteSizeLong());
}

void AccountWorker::login(AccountWorker& worker, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    std::shared_ptr<IM::Account::ImMsgLoginReq> pLoginReq =
            std::dynamic_pointer_cast<IM::Account::ImMsgLoginReq>(message.deserialize());
    if (pLoginReq == nullptr) {
        throw Base::Exception("Login message error");
    }

    UserInfoPtr pUserInfo = UserInfo::getUserInfo(*pLoginReq);
    if (nullptr == pUserInfo) {
        throw Base::Exception("Get login user info error");
    }

    // TODO:数据库获取用户密码，校验密码是否正确

    IM::Account::ImMsgLoginRes response;
    Poco::Timestamp timestamp;
    response.set_server_time(timestamp.epochMicroseconds());
    response.set_ret_code(IM::BaseType::ResultType::RESULT_TYPE_SUCCESS);
    worker.send(zmqMsg, response, response.ByteSizeLong());
}

void AccountWorker::registerUser(AccountWorker &worker, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    if (message.getTypeName() != "IM.Account.ImMsgRegisterReq") {
        throw Base::Exception("Message type error, name: " + message.getTypeName());
    }
    std::shared_ptr<IM::Account::ImMsgRegisterReq> pRegisterReq =
            std::dynamic_pointer_cast<IM::Account::ImMsgRegisterReq>(message.deserialize());
    UserInfoPtr pUserInfo = UserInfo::getUserInfo(pRegisterReq->status());
    if (nullptr == pUserInfo) {
        throw Base::Exception("Get register user info error");
    }

    // TODO:将用户信息保存进数据库，返回注册结果

    IM::Account::ImMsgRegisterRes registerRes;
    registerRes.set_allocated_msg_info(pRegisterReq->mutable_msg_info());
    registerRes.set_res_type(IM::BaseType::RESULT_TYPE_SUCCESS);
    worker.send(zmqMsg, registerRes, registerRes.ByteSizeLong());
}

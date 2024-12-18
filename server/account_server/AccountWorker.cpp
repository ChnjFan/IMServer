//
// Created by fan on 24-12-11.
//

#include "AccountWorker.h"
#include "Message.h"
#include "IM.BaseType.pb.h"
#include "IM.AccountServer.pb.h"
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
    it->second(*this, zmqMsg, message);
}

void AccountWorker::errRequest(AccountWorker &worker, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    IM::BaseType::ImMsgError errMsg;

    errMsg.set_error_type(IM::BaseType::RESULT_TYPE_REQUEST_ERR);

    char *data = new char[errMsg.ByteSizeLong()];
    errMsg.SerializeToArray(data, errMsg.ByteSizeLong());

    worker.send(zmqMsg, data, errMsg.ByteSizeLong());
    delete[] data;
}

void AccountWorker::login(AccountWorker& worker, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    IM::Account::ImMsgLoginRes response;
    Poco::Timestamp timestamp;
    //TODO: 登录终端uid设置
    response.set_uid("0");
    response.set_server_time(timestamp.epochMicroseconds());
    response.set_ret_code(IM::BaseType::ResultType::RESULT_TYPE_SUCCESS);

    int size = response.ByteSizeLong();
    char *content = new char[size];
    response.SerializeToArray(content, size);
    worker.send(zmqMsg, content, size);
    delete[] content;
}

void AccountWorker::registerUser(AccountWorker &worker, Base::ZMQMessage& zmqMsg, Base::Message &message) {
    UserInfoPtr pUserInfo = UserInfo::getUserInfo(message);
    if (nullptr == pUserInfo) {
        errRequest(worker, zmqMsg, message);
        return;
    }

    // TODO:将用户信息保存进数据库，返回注册结果

}

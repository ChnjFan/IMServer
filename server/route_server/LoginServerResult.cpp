//
// Created by fan on 24-11-24.
//

#include <memory>
#include "IMPdu.h"
#include "LoginServerResult.h"
#include "RouteConn.h"
#include "RouteConnManager.h"

LoginServerResult* LoginServerResult::instance = nullptr;

void LoginServerResult::handleLoginResult(Common::ByteStream &recvMsgBuf) {
    while (true) {
        std::shared_ptr<Common::IMPdu> pImPdu = Common::IMPdu::readPdu(recvMsgBuf);
        if (pImPdu == nullptr)
            return;

        RouteConn *pConn = RouteConnManager::getInstance()->getConn(pImPdu->getUuid());
        if (nullptr == pConn)
            continue;

        pConn->sendPdu(*pImPdu);
    }
}

LoginServerResult *LoginServerResult::getInstance() {
    if (instance == nullptr)
        instance = new LoginServerResult();
    return instance;
}

void LoginServerResult::registerLoginServer(Poco::Net::SocketReactor &reactor, Poco::ThreadPool& threadPool) {
    pConn = new LoginClientConn(reactor, &handleLoginResult);
    threadPool.start(*pConn);
}

void LoginServerResult::sendPdu(Common::IMPdu &imPdu) {
    pConn->sendPdu(imPdu);
}


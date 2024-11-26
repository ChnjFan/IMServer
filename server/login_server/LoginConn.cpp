//
// Created by fan on 24-11-26.
//

#include "LoginConn.h"
#include "IM.BaseType.pb.h"

LoginConn::LoginConn(const Poco::Net::StreamSocket &socket) : TcpConn(socket) {

}

void LoginConn::handleRecvMsg() {
    while (true) {
        std::shared_ptr<Common::IMPdu> pImPdu = Common::IMPdu::readPdu(getRecvMsgBuf());
        if (pImPdu == nullptr)
            return;

        switch (pImPdu->getMsgType()) {
            case IM::BaseType::MSG_TYPE_LOGIN:
                break;
            default:
                std::cout << "Msg " << pImPdu->getMsgType() << " error." << std::endl;
                break;
        }
    }
}

void LoginConn::handleTcpConnError() {

}

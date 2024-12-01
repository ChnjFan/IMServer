//
// Created by fan on 24-11-26.
//

#include "LoginConn.h"

LoginConn::LoginConn(const Poco::Net::StreamSocket &socket) : TcpConn(socket) {

}

void LoginConn::responseLogin(Common::IMPdu &imPdu, IM::BaseType::ResultType resultType) {
    IM::Login::ImMsgLoginRes loginRes;

    Poco::Timestamp timestamp;
    Poco::Timestamp::TimeVal epochMicroseconds = timestamp.epochMicroseconds();
    loginRes.set_server_time(epochMicroseconds);
    loginRes.set_ret_code(resultType);
    loginRes.set_uid(imPdu.getUuid());

    int size = loginRes.ByteSizeLong();
    Common::ByteStream buffer(size);
    if (!loginRes.SerializeToArray(buffer.getBuffer().data(), size)) {
        std::cout << "Serialize login response error" << std::endl;
    }

    Common::PDU_HEADER_DATA headerData = {static_cast<uint32_t>(size), imPdu.getMsgType(), imPdu.getMsgSeq(), {0}};
    memcpy(headerData.uuid, imPdu.getUuid().c_str(), PDU_HEADER_UUID_LEN);
    Common::PduHeader header(headerData);

    Common::IMPdu resPdu;
    resPdu.setImPdu(header, buffer);

    //回复结果
    sendPdu(resPdu);
}

void LoginConn::handleRecvMsg() {
    while (true) {
        std::shared_ptr<Common::IMPdu> pImPdu = Common::IMPdu::readPdu(getRecvMsgBuf());
        if (pImPdu == nullptr)
            return;

        switch (pImPdu->getMsgType()) {
            case IM::BaseType::MSG_TYPE_LOGIN_REQ:
                handleLogin(*pImPdu);
                break;
            default:
                std::cout << "Msg " << pImPdu->getMsgType() << " error." << std::endl;
                break;
        }
    }
}

void LoginConn::handleTcpConnError() {

}

void LoginConn::handleLogin(Common::IMPdu& imPdu) {
    IM::Login::ImMsgLoginReq loginReq;
    if (!loginReq.ParseFromArray(imPdu.getMsgBody().data(), imPdu.getMsgBody().size())) {
        /* 反解析失败，回复消息错误 */
        responseLogin(imPdu, IM::BaseType::RESULT_TYPE_REQUEST_ERR);
    }

    //TODO:查询数据库用户

    responseLogin(imPdu, IM::BaseType::RESULT_TYPE_SUCCESS);
}

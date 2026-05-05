//
// Created by Fan on 2026/5/4.
//

#include "VerifyGrpcClient.h"
#include "const.h"

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email) const {
    ClientContext context;
    GetVerifyRsp reply;
    GetVerifyReq request;

    request.set_email(email);
    if (const Status status = stub_->GetVerifyCode(&context, request, &reply); !status.ok()) {
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    }
    return reply;
}

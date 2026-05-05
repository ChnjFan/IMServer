//
// Created by Fan on 2026/5/4.
//

#ifndef IMSERVER_VERIFYGRPCCLIENT_H
#define IMSERVER_VERIFYGRPCCLIENT_H

#include <memory>
#include <grpcpp/grpcpp.h>

#include "message.pb.h"
#include "message.grpc.pb.h"

#include "Singleton.h"
#include "VerifyGrpcClient.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class VerifyGrpcClient : public Singleton<VerifyGrpcClient> {
public:
    [[nodiscard]] GetVerifyRsp GetVerifyCode(std::string email) const;

private:
    friend class Singleton<VerifyGrpcClient>;

    VerifyGrpcClient() {
        const std::shared_ptr<Channel> channel = grpc::CreateChannel("127.0.0.1:50051",
            grpc::InsecureChannelCredentials());
        stub_ = VerifyService::NewStub(channel);
    }

    std::unique_ptr<VerifyService::Stub> stub_;
};


#endif //IMSERVER_VERIFYGRPCCLIENT_H
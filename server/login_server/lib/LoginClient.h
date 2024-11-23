//
// Created by fan on 24-11-23.
//

#ifndef IMSERVER_LOGINCLIENT_H
#define IMSERVER_LOGINCLIENT_H

#include <grpcpp/grpcpp.h>
#include "IM.Login.grpc.pb.h"

class LoginClient {
public:
    explicit LoginClient(std::shared_ptr<grpc::Channel> channel);

private:
    std::unique_ptr<IM::Login::UserRequest::Stub> stub_;
};


#endif //IMSERVER_LOGINCLIENT_H

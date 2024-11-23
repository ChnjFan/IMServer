//
// Created by fan on 24-11-23.
//

#include "LoginClient.h"

LoginClient::LoginClient(std::shared_ptr<grpc::Channel> channel)
                        : stub_(IM::Login::UserRequest::NewStub(channel)) {

}



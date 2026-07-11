#ifndef IMSERVER_GRPC_TEST_CLIENT_H
#define IMSERVER_GRPC_TEST_CLIENT_H

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>

// Thin wrapper that owns a gRPC channel + stub for test use.
template<typename ServiceType>
class GrpcTestClient {
public:
    GrpcTestClient(const std::string& host, uint16_t port)
        : channel_(grpc::CreateChannel(host + ":" + std::to_string(port),
                   grpc::InsecureChannelCredentials())),
          stub_(ServiceType::NewStub(channel_)) {}

    std::unique_ptr<typename ServiceType::Stub>& stub() { return stub_; }

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<typename ServiceType::Stub> stub_;
};

#endif

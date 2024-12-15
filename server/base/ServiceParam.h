//
// Created by fan on 24-12-7.
//

#ifndef IMSERVER_SERVICEPARAM_H
#define IMSERVER_SERVICEPARAM_H

#include <string>
#include "Message.h"

namespace Base {

class ServiceParam {
public:
    ServiceParam(const char* serviceName,
                 std::string& publishEndpoint,
                 std::string& requestBindEndpoint,
                 std::string& requestConnectEndpoint,
                 int32_t threadPoolSize=8,
                 int32_t taskQueueSize=100)
                    : serviceName(serviceName)
                    , publishEndpoint(publishEndpoint)
                    , requestBindEndpoint(requestBindEndpoint)
                    , requestConnectEndpoint(requestConnectEndpoint)
                    , threadPoolSize(threadPoolSize)
                    , taskQueueSize(taskQueueSize) { }

    const std::string &getServiceName() const {
        return serviceName;
    }

    void setServiceName(const std::string &serviceName) {
        ServiceParam::serviceName = serviceName;
    }

    const std::string &getPublishEndport() const {
        return publishEndpoint;
    }

    void setPublishEndport(const std::string &publishEndpoint) {
        ServiceParam::publishEndpoint = publishEndpoint;
    }

    const std::string &getRequestEndpoint() const {
        return requestBindEndpoint;
    }

    void setRequestEndport(const std::string &requestEndpoint) {
        ServiceParam::requestBindEndpoint = requestEndpoint;
    }

    int32_t getThreadPoolSize() const {
        return threadPoolSize;
    }

    void setThreadPoolSize(int32_t threadPoolSize) {
        ServiceParam::threadPoolSize = threadPoolSize;
    }

    int32_t getTaskQueueSize() const {
        return taskQueueSize;
    }

    void setTaskQueueSize(int32_t taskQueueSize) {
        ServiceParam::taskQueueSize = taskQueueSize;
    }

    const std::string &getRequestConnectEndpoint() const {
        return requestConnectEndpoint;
    }

    void setRequestConnectEndpoint(const std::string &requestConnectEndpoint) {
        ServiceParam::requestConnectEndpoint = requestConnectEndpoint;
    }

private:
    std::string serviceName;
    std::string publishEndpoint;
    std::string requestBindEndpoint;
    std::string requestConnectEndpoint;

private:

    int32_t threadPoolSize;
    int32_t taskQueueSize;
};

}

#endif //IMSERVER_SERVICEPARAM_H

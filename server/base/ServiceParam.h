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
    ServiceParam(const char* serviceName="Service",
                 const char* servicePort="8251",
                 const char* routePort="8252",
                 int32_t threadPoolSize=8,
                 int32_t taskQueueSize=100)
                    : serviceName(serviceName)
                    , servicePort(servicePort)
                    , routePort(routePort)
                    , threadPoolSize(threadPoolSize)
                    , taskQueueSize(taskQueueSize) { }

    const std::string &getServiceName() const {
        return serviceName;
    }

    void setServiceName(const std::string &serviceName) {
        ServiceParam::serviceName = serviceName;
    }

    const std::string &getServicePort() const {
        return servicePort;
    }

    void setServicePort(const std::string &servicePort) {
        ServiceParam::servicePort = servicePort;
    }

    const std::string &getRoutePort() const {
        return routePort;
    }

    void setRoutePort(const std::string &routePort) {
        ServiceParam::routePort = routePort;
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

private:
    std::string serviceName;
    std::string servicePort;
    std::string routePort;

    int32_t threadPoolSize;
    int32_t taskQueueSize;
};

}

#endif //IMSERVER_SERVICEPARAM_H

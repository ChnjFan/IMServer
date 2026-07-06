//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_PRECHECKHANDLER_H
#define IMSERVER_PRECHECKHANDLER_H

#include "net/HttpConnection.h"

class PreCheckHandler {
public:
    void handle(std::shared_ptr<HttpConnection> conn);
};


#endif //IMSERVER_PRECHECKHANDLER_H

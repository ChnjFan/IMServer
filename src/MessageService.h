//
// Created by fan on 25-2-25.
//

#ifndef IMSERVER_MESSAGESERVICE_H
#define IMSERVER_MESSAGESERVICE_H

#include <string>
#include "Poco/Mutex.h"

class MessageService {
public:
    explicit MessageService();

private:
    mutable Poco::Mutex mutex_;
};


#endif //IMSERVER_MESSAGESERVICE_H

//
// Created by fan on 24-12-13.
//

#ifndef IMSERVER_MSGMONITOR_H
#define IMSERVER_MSGMONITOR_H

#include "Poco/Runnable.h"
#include "zmq.hpp"

class MsgMonitor : public Poco::Runnable {
public:
    explicit MsgMonitor(zmq::context_t& context);

    void run() override;

private:
    zmq::context_t& context;
};


#endif //IMSERVER_MSGMONITOR_H

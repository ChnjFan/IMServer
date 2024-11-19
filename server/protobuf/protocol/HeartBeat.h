//
// Created by fan on 24-11-19.
//

#ifndef IMSERVER_HEARTBEAT_H
#define IMSERVER_HEARTBEAT_H

#include "Poco/Util/Timer.h"
#include "Poco/Util/TimerTask.h"
#include "Poco/Net/StreamSocket.h"

class HeartBeatTask : public Poco::Util::TimerTask {
public:
    explicit HeartBeatTask(Poco::Net::StreamSocket &socket);
    void run() override;

private:
    Poco::Net::StreamSocket socket;
};

class HeartBeat {
public:
    explicit HeartBeat(Poco::Net::StreamSocket &socket, long startInterval=DEFAULT_START_INTERVAL, long periodicInterval=DEFAULT_PERIODIC_INTERVAL);
    ~HeartBeat();

private:
    static constexpr long DEFAULT_START_INTERVAL = 60 * 1000;
    static constexpr long DEFAULT_PERIODIC_INTERVAL = 60 * 1000;

    Poco::AutoPtr<HeartBeatTask> heartBeatTask;
    Poco::Util::Timer timer;
};


#endif //IMSERVER_HEARTBEAT_H

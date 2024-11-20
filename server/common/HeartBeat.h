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
    HeartBeatTask() = default;
    void run() override;
};

class HeartBeat {
public:
    explicit HeartBeat(long startInterval=DEFAULT_START_INTERVAL, long periodicInterval=DEFAULT_PERIODIC_INTERVAL);
    ~HeartBeat();

    static constexpr Poco::Timespan::TimeDiff HEARTBEAT_CHECK_TIME = 5 * 60 * 1000;

private:
    static constexpr long DEFAULT_START_INTERVAL = 60 * 1000;
    static constexpr long DEFAULT_PERIODIC_INTERVAL = 60 * 1000;


    Poco::AutoPtr<HeartBeatTask> heartBeatTask;
    Poco::Util::Timer timer;
};


#endif //IMSERVER_HEARTBEAT_H

//
// Created by fan on 24-11-22.
//

#ifndef IMSERVER_HEARTBEATHANDLER_H
#define IMSERVER_HEARTBEATHANDLER_H

#include "Poco/Util/TimerTask.h"
#include "Poco/Util/Timer.h"
#include "Poco/Timespan.h"

class HeartBeatHandler : public Poco::Util::TimerTask {
public:
    HeartBeatHandler() = default;
    void run() override;

    static constexpr Poco::Timespan::TimeDiff HEARTBEAT_CHECK_TIME = 5 * 1000;
};

#endif //IMSERVER_HEARTBEATHANDLER_H

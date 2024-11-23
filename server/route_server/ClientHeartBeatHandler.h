//
// Created by fan on 24-11-22.
//

#ifndef IMSERVER_CLIENTHEARTBEATHANDLER_H
#define IMSERVER_CLIENTHEARTBEATHANDLER_H

#include "Poco/Util/TimerTask.h"
#include "Poco/Util/Timer.h"
#include "Poco/Timespan.h"

class ClientHeartBeatHandler : public Poco::Util::TimerTask {
public:
    ClientHeartBeatHandler() = default;
    void run() override;

    static constexpr Poco::Timespan::TimeDiff HEARTBEAT_CHECK_TIME = 5 * 1000;
};

#endif //IMSERVER_CLIENTHEARTBEATHANDLER_H

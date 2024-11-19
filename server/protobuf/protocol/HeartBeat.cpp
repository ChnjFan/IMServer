//
// Created by fan on 24-11-19.
//

#include "HeartBeat.h"
#include <iostream>

HeartBeatTask::HeartBeatTask(Poco::Net::StreamSocket &socket) : socket(socket) {

}

void HeartBeatTask::run() {
    // TODO: 发送心跳包
    std::cout << "Send heart beat in thread: " << Poco::Thread::current()->name() << std::endl;
}

HeartBeat::HeartBeat(Poco::Net::StreamSocket &socket, long startInterval, long periodicInterval)
                        : heartBeatTask(new HeartBeatTask(socket)), timer() {
    timer.schedule(heartBeatTask, startInterval, periodicInterval);
}

HeartBeat::~HeartBeat() {
    heartBeatTask->cancel();
}

//
// Created by fan on 24-11-19.
//

#include "HeartBeat.h"
#include <iostream>

void HeartBeatTask::run() {
    // TODO: 轮询所有的连接检查是否超时
    std::cout << "Check all conn heart beat in thread: " << Poco::Thread::current()->name() << std::endl;
}

HeartBeat::HeartBeat(long startInterval, long periodicInterval)
                        : heartBeatTask(), timer() {
    timer.schedule(heartBeatTask, startInterval, periodicInterval);
}

HeartBeat::~HeartBeat() {
    heartBeatTask->cancel();
}

//
// Created by fan on 24-11-22.
//

#include <iostream>
#include "HeartBeatHandler.h"
#include "SessionConnManager.h"

void HeartBeatHandler::run() {
    std::cout << "Check Route conn time tick in thread: " << Poco::Thread::current()->name() << std::endl;
    SessionConnManager::getInstance()->checkTimeStamp();
}

void HeartBeatHandlerImpl::start() {
    timer.schedule(&heartBeatTask, delay, interval);
}

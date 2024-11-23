//
// Created by fan on 24-11-22.
//

#include <iostream>
#include "ClientHeartBeatHandler.h"
#include "RouteConnManager.h"

void ClientHeartBeatHandler::run() {
    std::cout << "Check Route conn time tick in thread: " << Poco::Thread::current()->name() << std::endl;
    RouteConnManager::getInstance()->checkTimeStamp();
}

//
// Created by fan on 25-2-25.
//

#include "MessageService.h"

MessageService::MessageService(unsigned short port) : socket(port), reactor(), acceptor(socket, reactor) {

}

void MessageService::start() {
    reactor.run();
}

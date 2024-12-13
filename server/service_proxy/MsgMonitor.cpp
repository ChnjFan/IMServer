//
// Created by fan on 24-12-13.
//

#include "MsgMonitor.h"

MsgMonitor::MsgMonitor(zmq::context_t &context)
                        : context(context) {

}

void MsgMonitor::run() {
    zmq::socket_t listener(context, ZMQ_PAIR);
    listener.connect("inproc://listener");
    std::cout << "Monitor on " << listener.get(zmq::sockopt::last_endpoint) << std::endl;

    while (true) {
        zmq::message_t message;
        if (listener.recv(message)) {
            std::string msg = std::string((char*)(message.data()), message.size());
            std::cout << "Listener Received: " << std::endl;
            if (msg[0] == 0 || msg[0] == 1){
                std::cout << int(msg[0]);
                std::cout << msg[1]<< std::endl;
            } else {
                std::cout << msg << std::endl;
            }
        }
    }
}

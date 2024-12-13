//  Pathological subscriber
//  Subscribes to one random topic and prints received messages

#include "zmq.hpp"
#include <iostream>

int main (int argc, char *argv [])
{
    zmq::context_t context(1);
    zmq::socket_t subscriber (context, ZMQ_SUB);

    //  Initialize random number generator
    srandom ((unsigned) time (NULL));

    if (argc == 2)
        subscriber.connect(argv [1]);
    else
        subscriber.connect("tcp://localhost:5558");

    std::stringstream ss;
    ss << std::dec << std::setw(3) << std::setfill('0');
    std::cout << "topic:" << ss.str() << std::endl;

    subscriber.set( zmq::sockopt::subscribe, ss.str().c_str());

    while (1) {
        zmq::message_t message;
        subscriber.recv(message);
        std::string topic = std::string((char*)(message.data()), message.size());
        subscriber.recv(message);
        std::string data = std::string((char*)(message.data()), message.size());
        std::cout << topic << data << std::endl;
    }
    return 0;
}
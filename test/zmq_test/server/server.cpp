//  Pathological publisher
//  Sends out 1,000 topics and then one random update per second

#include <thread>
#include <chrono>
#include "zmq.hpp"

int main (int argc, char *argv [])
{
    zmq::context_t context(1);
    zmq::socket_t publisher(context, ZMQ_PUB);

    //  Initialize random number generator
    srandom ((unsigned) time (NULL));

    if (argc == 2)
        publisher.bind(argv [1]);
    else
        publisher.bind("tcp://*:5557");

    //  Ensure subscriber connection has time to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //  Send out all 1,000 topic messages
//    int topic_nbr;
//    for (topic_nbr = 0; topic_nbr < 1000; topic_nbr++) {
//        std::stringstream ss;
//        ss << std::dec << std::setw(3) << std::setfill('0') << topic_nbr;
//        zmq::message_t message(ss.str());
//        publisher.send(message, zmq::send_flags::sndmore);
//        message.rebuild(std::string("Save Roger"));
//        publisher.send(message, zmq::send_flags::none);
//    }

    //  Send one random update per second

        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::stringstream ss;
        ss << std::dec << std::setw(3) << std::setfill('0');

        zmq::message_t message(ss.str());
        message.rebuild(std::string("Save Roger"));
        publisher.send(message, zmq::send_flags::sndmore);
        message.rebuild(std::string("Off with his head!"));
        publisher.send(message, zmq::send_flags::none);
    while (1) {

    }
    return 0;
}
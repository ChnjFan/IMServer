//  Last value cache
//  Uses XPUB subscription messages to re-send data

#include <unordered_map>
#include <iostream>
#include "zmq.hpp"

int main ()
{
    zmq::context_t context(1);
    zmq::socket_t  frontend(context, ZMQ_SUB);
    zmq::socket_t  backend(context, ZMQ_XPUB);

    frontend.connect("tcp://localhost:5557");
    backend.bind("tcp://*:5558");

    //  Subscribe to every single topic from publisher
    frontend.set(zmq::sockopt::subscribe, "");

    //  Store last instance of each topic in a cache
    std::unordered_map<std::string, std::string> cache_map;

    zmq::pollitem_t items[2] = {
            { static_cast<void*>(frontend), 0, ZMQ_POLLIN, 0 },
            { static_cast<void*>(backend), 0, ZMQ_POLLIN, 0 }
    };

    //  .split main poll loop
    //  We route topic updates from frontend to backend, and we handle
    //  subscriptions by sending whatever we cached, if anything:
    while (1)
    {
        if (zmq::poll(items, 2, 1000) == -1)
            break; //  Interrupted

        //  Any new topic data we cache and then forward
        if (items[0].revents & ZMQ_POLLIN)
        {
            zmq::message_t message;
            frontend.recv(message);
            std::string topic = std::string((char*)(message.data()), message.size());
            frontend.recv(message);
            std::string data = std::string((char*)(message.data()), message.size());

            std::cout << topic << data << std::endl;

            if (topic.empty())
                break;

            cache_map[topic] = data;

            message.rebuild(topic);
            backend.send(message, zmq::send_flags::sndmore);
            message.rebuild(data);
            backend.send(message, zmq::send_flags::none);
        }

        //  .split handle subscriptions
        //  When we get a new subscription, we pull data from the cache:
        if (items[1].revents & ZMQ_POLLIN) {
            zmq::message_t msg;

            backend.recv(&msg);
            if (msg.size() == 0)
                break;

            //  Event is one byte 0=unsub or 1=sub, followed by topic
            uint8_t *event = (uint8_t *)msg.data();
            if (event[0] == 1) {
                std::string topic((char *)(event+1), msg.size()-1);

                auto i = cache_map.find(topic);
                if (i != cache_map.end())
                {
                    zmq::message_t message;
                    message.rebuild(topic);
                    backend.send(message, zmq::send_flags::sndmore);
                    message.rebuild(i->second);
                    backend.send(message, zmq::send_flags::none);
                }
            }
        }
    }

    return 0;
}
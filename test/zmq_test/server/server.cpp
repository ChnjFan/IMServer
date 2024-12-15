//  Pathological publisher
//  Sends out 1,000 topics and then one random update per second

#include <thread>
#include <vector>
#include <memory>
#include <functional>
#include <csignal>
#include "zmq.hpp"
#include <iostream>

class server_worker {
public:
    server_worker(zmq::context_t &ctx, int sock_type)
            : ctx_(ctx),
              worker_(ctx_, sock_type)
    {}

    void work() {
        worker_.connect("inproc://backend");

        try {
            while (true) {
                zmq::message_t identity;
                zmq::message_t msg;
                zmq::message_t copied_id;
                zmq::message_t copied_msg;
                worker_.recv(&identity);
                worker_.recv(&msg);

                std::cout << identity.to_string() << msg.to_string() << std::endl;

                for (int reply = 0; reply < 5; ++reply) {
                    sleep(1000);
                    copied_id.copy(&identity);
                    copied_msg.copy(&msg);
                    worker_.send(copied_id, ZMQ_SNDMORE);
                    worker_.send(copied_msg);
                }
            }
        }
        catch (std::exception &e) {}
    }



private:
    zmq::context_t &ctx_;
    zmq::socket_t worker_;
};

class server_pub {
public:
    server_pub(zmq::context_t &ctx, int sock_type) : publisher(ctx, sock_type) { }

    void work() {
        publisher.bind("tcp://*:5571");

        while (1) {
            std::string str("publish service");
            zmq::message_t msg(str);
            publisher.send(msg, zmq::send_flags::none);
            sleep(3);
        }
    }
private:
    zmq::socket_t publisher;
};

int main (int argc, char *argv [])
{
    zmq::context_t ctx_(1);
    zmq::socket_t frontend_(ctx_, ZMQ_ROUTER);
    zmq::socket_t backend_(ctx_, ZMQ_DEALER);

    frontend_.bind("tcp://*:5570");
    backend_.bind("inproc://backend");

    std::vector<server_worker *> worker;
    std::vector<std::thread *> worker_thread;
    for (int i = 0; i < 5; ++i) {
        worker.push_back(new server_worker(ctx_, ZMQ_DEALER));

        worker_thread.push_back(new std::thread(std::bind(&server_worker::work, worker[i])));
        worker_thread[i]->detach();
    }

    server_pub publish(ctx_, ZMQ_PUB);
    std::thread *pub = new std::thread(std::bind(&server_pub::work, &publish));
    pub->detach();

    try {
        // 使用句柄和from_handle标签构造一个socket_ref
        zmq::proxy(zmq::socket_ref(zmq::from_handle, frontend_.handle()),
                   zmq::socket_ref(zmq::from_handle, backend_.handle()));
    }
    catch (std::exception &e) {}

    for (int i = 0; i < 5; ++i) {
        delete worker[i];
        delete worker_thread[i];
    }

    return 0;
}
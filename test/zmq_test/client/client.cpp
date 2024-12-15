//  Pathological subscriber
//  Subscribes to one random topic and prints received messages

#include <vector>
#include <thread>
#include <memory>
#include <functional>
#include <iostream>
#include <zmq.hpp>

class client_task {
public:
    client_task()
            : ctx_(1),
              client_socket_(ctx_, ZMQ_DEALER)
    {}

    void start() {
        // generate random identity
        char identity[10] = {};
        sprintf(identity, "04-04");
        printf("%s\n", identity);
        client_socket_.connect("tcp://localhost:5570");

        zmq::pollitem_t items[] = {
                { client_socket_, 0, ZMQ_POLLIN, 0 } };
        int request_nbr = 0;
        try {
            while (true) {
                for (int i = 0; i < 100; ++i) {
                    // 10 milliseconds
                    zmq::poll(items, 1, 10);
                    if (items[0].revents & ZMQ_POLLIN) {
                        printf("\n%s ", identity);
                    }
                }
                char request_string[16] = {};
                sprintf(request_string, "request #%d", ++request_nbr);
                zmq::message_t message("topic", 5);
                client_socket_.send(message, zmq::send_flags::sndmore);
                message.rebuild(request_string, strlen(request_string));
                client_socket_.send(message, zmq::send_flags::none);
            }
        }
        catch (std::exception &e) {}
    }

private:
    zmq::context_t ctx_;
    zmq::socket_t client_socket_;
};

int main (int argc, char *argv [])
{
    client_task ct1;
    client_task ct2;
    client_task ct3;
    zmq::context_t ctx_(1);

    zmq::socket_t sub(ctx_, zmq::socket_type::sub);
    sub.connect("tcp://localhost:5571");
    sub.set(zmq::sockopt::subscribe, "");

    while (1) {
        zmq::message_t msg;
        sub.recv(msg, zmq::recv_flags::none);
        if (msg.to_string().length() != 0) {
            std::cout << msg.to_string() << std::endl;
            break;
        }
    }

    std::thread t1(std::bind(&client_task::start, &ct1));
    std::thread t2(std::bind(&client_task::start, &ct2));
    std::thread t3(std::bind(&client_task::start, &ct3));

    t1.detach();
    t2.detach();
    t3.detach();

    getchar();
    return 0;
}
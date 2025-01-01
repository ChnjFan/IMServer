//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVERWORKER_H
#define IMSERVER_SERVERWORKER_H

#include "Poco/Runnable.h"
#include "Message.h"

namespace ServerNet {

class ServerWorker : public Poco::Runnable {
public:
    void run() override;

    static void send(std::string connName, Base::Message& message);
    static void close(std::string connName);

    virtual void work(Base::Message& message, std::string& connName);
private:
};

}

#endif //IMSERVER_SERVERWORKER_H

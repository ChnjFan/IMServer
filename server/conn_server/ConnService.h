//
// Created by fan on 24-12-30.
//

#ifndef IMSERVER_CONNSERVICE_H
#define IMSERVER_CONNSERVICE_H

#include "ServerWorker.h"

class ConnService : public ServerNet::ServerWorker {
public:
    void work(Base::Message& message, std::string& connName) override;
private:
};


#endif //IMSERVER_CONNSERVICE_H

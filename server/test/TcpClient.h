//
// Created by fan on 24-11-28.
//

#ifndef IMSERVER_TCPCLIENT_H
#define IMSERVER_TCPCLIENT_H

#include <string>
#include "IMPdu.h"

class TcpClient {
public:
    TcpClient(std::string& serverIP, int port);

    void sendPdu(Common::IMPdu &imPdu);
    void readPdu();
};


#endif //IMSERVER_TCPCLIENT_H

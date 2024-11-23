/**
 * @file client_conn.cpp
 * @brief 客户端连接测试
 */

#include <iostream>
#include <string>
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "IMPdu.h"

#define BUFFER_SIZE 1024
using Poco::Net::SocketAddress;
using Poco::Net::StreamSocket;
using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

int main (int argc, const char * argv[])
{
    AutoPtr<IniFileConfiguration> pConfig(new IniFileConfiguration("route_server_config.ini"));

    std::string serverIP = "127.0.0.1";
    int serverPort = pConfig->getInt("server.listen_port");

    SocketAddress address(serverIP, serverPort);
    StreamSocket socket(address);
    char buffer[BUFFER_SIZE] = {0};
    int sendlen = socket.sendBytes(buffer, 12);
    std::cout << "send size: " << sendlen << std::endl;

    ByteStream data(1024);
    // 循环接收数据
    while (true)
    {
        // 检查是否有数据可读
        if (socket.available())
        {
            // 接收数据
            int len = socket.receiveBytes(buffer, BUFFER_SIZE);
            if (len <= 0)
                break;

            data.write(buffer, len);
            // 输出接收到的数据
            std::cout << "recv size: " << data.peekUint32() << std::endl;
        }
    }

    socket.close();
    return 0;
}
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

#define BUFFER_SIZE 1024
using Poco::Net::SocketAddress;
using Poco::Net::StreamSocket;
using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

int main (int argc, const char * argv[])
{
    AutoPtr<IniFileConfiguration> pConfig(new IniFileConfiguration("../route_server/bin/route_server_config.ini"));

    std::string serverIP = pConfig->getString("server.listen_ip");
    int serverPort = pConfig->getInt("server.listen_port");

    SocketAddress address(serverIP, serverPort);
    StreamSocket socket(address);
    char buffer[BUFFER_SIZE];
    // 循环接收数据
    while (true)
    {
        // 检查是否有数据可读
        if (socket.available())
        {
            // 接收数据
            int len = socket.receiveBytes(buffer, BUFFER_SIZE);
            // 确保接收到的数据是字符串形式
            buffer[len] = '\0';
            // 输出接收到的数据
            std::cout << "" << buffer << std::endl;
        }
    }
    return 0;
}
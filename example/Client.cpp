//
// Created by fan on 25-2-26.
//

#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"

int main() {
    try
    {
        // 服务器地址和端口
        Poco::Net::SocketAddress serverAddress("127.0.0.1", 10001);

        // 创建并连接到服务器
        Poco::Net::StreamSocket socket(serverAddress);
        std::cout << "Connected to server: " << serverAddress.host().toString() << ":" << serverAddress.port() << std::endl;

        // 发送数据到服务器
        const char* sendData = "Hello from client!";
        socket.sendBytes(sendData, std::strlen(sendData));
        std::cout << "Sent data: " << sendData << std::endl;

        // 接收服务器响应
        char receiveBuffer[1024];
        int receivedBytes = socket.receiveBytes(receiveBuffer, sizeof(receiveBuffer));
        if (receivedBytes > 0)
        {
            receiveBuffer[receivedBytes] = '\0';
            std::cout << "Received data: " << receiveBuffer << std::endl;
        }

        // 关闭连接
        socket.close();
    }
    catch (const Poco::Net::NetException& e)
    {
        std::cerr << "Network error: " << e.what() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
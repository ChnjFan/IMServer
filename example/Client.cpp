//
// Created by fan on 25-2-26.
//

#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/ByteOrder.h"
#include "Poco/Checksum.h"
#include "IM.ServerMsg.pb.h"

int main() {
    try
    {
        // 服务器地址和端口
        Poco::Net::SocketAddress serverAddress("127.0.0.1", 10001);

        // 创建并连接到服务器
        Poco::Net::StreamSocket socket(serverAddress);
        std::cout << "Connected to server: " << serverAddress.host().toString() << ":" << serverAddress.port() << std::endl;

        // 发送数据到服务器
        char sendBuffer[1024] = {0};
        IM::ServerMsg::LoginRequest login;
        login.set_account_id("admin");
        login.set_password("admin");
        Poco::Timestamp currentTimestamp;
        login.set_timestamp(std::to_string(currentTimestamp.epochTime()));

        std::string type_name = IM::ServerMsg::LoginRequest::descriptor()->full_name();
        std::cout << type_name;

        // 保存消息
        char *type = &sendBuffer[4];
        strncpy(type, "IM.ServerMsg.LoginRequest", 1024);
        char *data = type + strlen("IM.ServerMsg.LoginRequest") + 1;
        login.SerializeToArray(data, 1024);

        // 设置长度
        int32_t size = strlen("IMServerMsg.LoginRequest") + 1 + login.ByteSizeLong();
        int32_t size_net = Poco::ByteOrder::toNetwork(size);
        memcpy(sendBuffer, &size_net, sizeof(int32_t));

        Poco::Checksum crc32(Poco::Checksum::TYPE_CRC32);
        crc32.update(type, size);
        uint32_t checksum = crc32.checksum();
        uint32_t checksum_net = Poco::ByteOrder::toNetwork(checksum);
        memcpy(type + size, &checksum_net, sizeof(uint32_t));

        socket.sendBytes(sendBuffer, 1024);
        std::cout << "Sent data: " << sendBuffer << std::endl;

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
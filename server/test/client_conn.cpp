#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "IM.AccountServer.pb.h"

#define IM_CONNECT_SERVER_IP    "127.0.0.1"
#define IM_CONNECT_SERVER_PORT  8200

int main()
{
    Poco::Net::SocketAddress socket_addr(IM_CONNECT_SERVER_IP, IM_CONNECT_SERVER_PORT);
    Poco::Net::StreamSocket socket(socket_addr);

    IM::Account::ImMsgLoginReq loginReq;

    loginReq.set_email("test@163.com");
    loginReq.set_password("TEST_PASSWORD_MD5");

    int bodySize = loginReq.ByteSizeLong();
    std::string typeName = "IM.Account.ImMsgLoginReq";
    uint32_t len = typeName.length() + 1;
    int size = bodySize + len + sizeof(uint32_t) * 2;

    char *pBuf = new char[size];
    memcpy(pBuf, &size, sizeof(uint32_t));
    memcpy(pBuf + sizeof(uint32_t), &len, sizeof(uint32_t));
    memcpy(pBuf + 2 * sizeof(uint32_t), typeName.data(), len);
    loginReq.SerializeToArray(pBuf + 2 * sizeof(uint32_t) + len, bodySize);
    socket.sendBytes(pBuf, size);

    char buffer[1024];
    int res = socket.receiveBytes(buffer, sizeof(buffer));
    buffer[res] = '\0';
    std::cout << "Received from server: " << buffer << std::endl;

    socket.close();
    return 0;
}
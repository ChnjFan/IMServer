//
// Created by fan on 25-1-5.
//

#include "ZookeeperClient.h"
#include <semaphore.h>
#include "Exception.h"

void global_watcher(zhandle_t *zh, int type,
                    int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT)  // 回调的消息类型是和会话相关的消息类型
    {
        if (state == ZOO_CONNECTED_STATE)  // zkclient和zkserver连接成功
        {
            sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
        }
    }
}

ServerNet::ZookeeperClient::ZookeeperClient() : pHandle(nullptr) {

}

ServerNet::ZookeeperClient::~ZookeeperClient() {
    if (nullptr != pHandle)
        zookeeper_close(pHandle);
}

void ServerNet::ZookeeperClient::start(ServerNet::ApplicationConfig &config) {
    std::string host = config.getConfig()->getString("zookeeper.ip");
    std::string port = config.getConfig()->getString("zookeeper.port");
    std::string conn = host + ":" + port;

    pHandle = zookeeper_init(conn.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if (nullptr == pHandle) {
        std::cout << "zookeeper_init error" << std::endl;
        throw Base::Exception("zookeeper_init error");
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(pHandle, &sem); //给这个句柄添加一些额外的信息

    sem_wait(&sem); //阻塞结束后才连接成功！！！
    std::cout << "zookeeper_init success!" << std::endl;
}

void ServerNet::ZookeeperClient::create(const char *path, const char *data, int datalen, int state) {
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;

    flag = zoo_exists(pHandle, path, 0, nullptr);
    if (ZNONODE == flag)
    {
        flag = zoo_create(pHandle, path, data, datalen,
                          &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK)
        {
            std::cout << "znode create success... path:" << path << std::endl;
        }
        else
        {
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error... path:" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

std::string ServerNet::ZookeeperClient::getData(const char *path) {
    char buffer[64];
    int bufferLen = sizeof(buffer);
    int flag = zoo_get(pHandle, path, 0, buffer, &bufferLen, nullptr);
    if (flag != ZOK)
    {
        std::cout << "get znode error... path:" << path << std::endl;
        return "";
    }
    else
    {
        return buffer;
    }
}





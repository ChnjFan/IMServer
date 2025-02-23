//
// Created by fan on 25-1-5.
//

#ifndef IMSERVER_ZOOKEEPERCLIENT_H
#define IMSERVER_ZOOKEEPERCLIENT_H
#define THREADED
#include <string>
#include "zookeeper/zookeeper.h"
#include "ApplicationConfig.h"

namespace ServerNet {

class ZookeeperClient {
public:
    ZookeeperClient();

    ~ZookeeperClient();
    void start(ApplicationConfig& config);
    void create(const char *path, const char *data, int datalen, int state=0);
    std::string getData(const char *path);
private:
    zhandle_t *pHandle;
};

}

#endif //IMSERVER_ZOOKEEPERCLIENT_H

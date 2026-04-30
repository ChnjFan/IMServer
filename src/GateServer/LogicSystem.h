//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_LOGICSYSTEM_H
#define IMSERVER_LOGICSYSTEM_H

#include <unordered_map>

#include "Singleton.h"
#include "HttpConnection.h"

class HttpConnection;

typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpRequestCallback;

class LogicSystem : public Singleton<LogicSystem> {
public:
    ~LogicSystem();
    bool handleGet(std::string url, std::shared_ptr<HttpConnection> connection);
    void registerGet(const std::string& url, const HttpRequestCallback& handler);

private:
    friend class Singleton<LogicSystem>;
    LogicSystem();

    std::unordered_map<std::string, HttpRequestCallback> postHandlers_;
    std::unordered_map<std::string, HttpRequestCallback> getHandlers_;
};


#endif //IMSERVER_LOGICSYSTEM_H
//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCELOGICSYSTEM_H
#define IMSERVER_RESOURCELOGICSYSTEM_H

#include <functional>
#include <unordered_map>

#include "Singleton.h"
#include "../net/HttpConnection.h"

class HttpConnection;

typedef std::function<void(std::shared_ptr<HttpConnection>)> ResourceRequestCallback;

class ResourceLogicSystem : public Singleton<ResourceLogicSystem> {
public:
    ~ResourceLogicSystem() = default;

    bool handleGet(const std::string& path, const std::shared_ptr<HttpConnection>& connection);
    void registerGet(const std::string& path, const ResourceRequestCallback& handler);

    bool handlePost(const std::string& path, const std::shared_ptr<HttpConnection>& connection);
    void registerPost(const std::string& path, const ResourceRequestCallback& handler);

private:
    friend class Singleton<ResourceLogicSystem>;
    ResourceLogicSystem();

    std::unordered_map<std::string, ResourceRequestCallback> postHandlers_;
    std::unordered_map<std::string, ResourceRequestCallback> getHandlers_;
};


#endif //IMSERVER_RESOURCELOGICSYSTEM_H

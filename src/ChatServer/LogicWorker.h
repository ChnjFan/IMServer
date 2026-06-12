//
// Created by Fan on 2026/6/10.
//

#ifndef IMSERVER_LOGICWORKER_H
#define IMSERVER_LOGICWORKER_H

#include <functional>

#include "ThreadPool.h"
#include "Session.h"

using logicWorkerHandler = std::function<void()>;

class LogicWorker final : public Task {
public:
    LogicWorker(std::shared_ptr<Session> session, uint16_t msgId, const std::string& data);
    ~LogicWorker() override = default;
    void exec() override;
    void init();
private:
    void registerHandler(uint16_t msgId, const logicWorkerHandler& handler);

    void fileUploadHandler();

    std::shared_ptr<Session> session_;
    uint16_t msgId_;
    std::string data_;

    std::unordered_map<uint16_t, logicWorkerHandler> handlers_;
};


#endif //IMSERVER_LOGICWORKER_H

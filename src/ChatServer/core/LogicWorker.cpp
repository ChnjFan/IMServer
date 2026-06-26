//
// Created by Fan on 2026/6/10.
//

#include "LogicWorker.h"

#include <fstream>
#include <filesystem>
#include <json/json.h>
#include <cerrno>

#include "ConfigMgr.h"
#include "const.h"

LogicWorker::LogicWorker(std::shared_ptr<Session> session, uint16_t msgId, const std::string &data)
    : session_(session), msgId_(msgId), data_(data)
{
}

void LogicWorker::exec() {
    if (handlers_.find(msgId_) == handlers_.end()) {
        std::cout << "Msg id [" << msgId_ << "] handler not found" << std::endl;
        return;
    }
    handlers_[msgId_]();
}

void LogicWorker::init() {
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_UPLOAD_FILE_REQ), [this]() {
        return fileUploadHandler();
    });
}

void LogicWorker::registerHandler(uint16_t msgId, const logicWorkerHandler &handler) {
    if (handlers_.find(msgId) != handlers_.end()) {
        return;
    }
    handlers_.insert({msgId, handler});
}

/**
 * 消息格式：
 *     'conv_id': c2c_,     文件存储在会话目录中，例如 static/c2c_/file.txt
 *     'name': file name,
 *     'md5': md5,
 *     'seq': 1,
 *     'trans_size': 1024,
 *     'total_size': 2048
 *     'data': file content,
 *     'last': 0/1,
 *  回复消息使用 recv_size 表示已接收大小， ack = seq + 1 请求下一包
 *  上传文件元信息已经在聊天消息里发送，这里只需要上传文件就可以，不需要知道发给谁
 */
void LogicWorker::fileUploadHandler() {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, this]() {
        const std::string jsonStr = root.toStyledString();
        session_->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_UPLOAD_FILE_RSP));
    });
    if (Json::Reader reader; !reader.parse(data_, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    const auto data = srcRoot["data"].asString();
    auto content = base64_decode(data);

    auto seq = srcRoot["seq"].asInt();
    auto name = srcRoot["name"].asString();
    auto convId = srcRoot["conv_id"].asString();

    // 创建目录
    auto& config = ConfigMgr::getInstance();
    const std::filesystem::path currentDir = std::filesystem::current_path();
    auto path = currentDir / std::filesystem::path(config["Static"]["Path"] + "/" + convId + "/");
    const auto filePath = path / std::filesystem::path(name);
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        std::cout << "Failed to create directory " << path << " reason: " << ec.message() << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
        return;
    }

    std::ofstream file;
    if (seq == 1) {
        file.open(filePath, std::ios::trunc | std::ios::binary);
    }
    else {
        file.open(filePath, std::ios::app | std::ios::binary);
    }

    if (!file.is_open()) {
        std::cout << "Failed to open file: " << filePath << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
        return;
    }

    file.write(content.data(), content.size());
    if (file.fail()) {
        std::cout << "Failed to write file: " << filePath
            << " error: " << errno << " reason: " << std::strerror(errno) << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
        return;
    }

    file.close();

    root["conv_id"] = convId;
    root["name"] = name;
    root["seq"] = seq + 1; // 获取下一包数据
    root["total_size"] = srcRoot["total_size"].asInt();
    root["recv_size"] = srcRoot["trans_size"].asInt();
}

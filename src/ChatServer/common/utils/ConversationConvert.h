//
// Created by Fan on 2026/7/1.
//

#ifndef IMSERVER_CONVERSATIONCONVERT_H
#define IMSERVER_CONVERSATIONCONVERT_H

#include <regex>
#include <string>

inline int getOtherUid(const std::string &convId, const int uid) {
    static const std::regex reg(R"(^c2c_(\d+)_(\d+))");
    std::smatch matched;
    if (!std::regex_match(convId, matched, reg))
        return -1; // 格式非法

    const int u1 = std::stoi(matched[1].str());
    const int u2 = std::stoi(matched[2].str());
    if (u1 != uid && u2 != uid) {// 会话要包含传入的 uid
        return -1;
    }
    return (u1 == uid) ? u2 : u1;
}

#endif //IMSERVER_CONVERSATIONCONVERT_H
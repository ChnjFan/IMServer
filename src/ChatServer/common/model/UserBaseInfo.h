//
// Created by Fan on 2026/6/23.
//

#ifndef IMSERVER_USERBASEINFO_H
#define IMSERVER_USERBASEINFO_H

#include <string>
#include <unordered_set>
#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

enum class GenderType : int8_t {
    UNKNOWN = 0,
    MALE = 1,
    FEMALE = 2,
};

struct UserBaseInfo {
    int id = -1;
    int uid = -1;
    int8_t gender = 0;
    int8_t pad[3];
    std::optional<std::string> name;
    std::optional<std::string> email;
    std::optional<std::string> password;
    std::optional<std::string> salt;
    std::optional<std::string> avatarUrl;
    std::optional<std::string> createTime;
    std::optional<std::string> updateTime;

    inline static std::unordered_set<std::string> keys{"id", "uid", "gender", "name",
    "email", "password", "salt", "avatar_url", "create_time", "update_time"};

    static UserBaseInfo fromResult(const std::shared_ptr<sql::ResultSet>& result);

    void fromJson(Json::Value& value);
    void fromSqlResult(const std::shared_ptr<sql::ResultSet>& result);
    void toJson(Json::Value& value) const;

    [[nodiscard]] std::string getSearchProperty() const;
    [[nodiscard]] std::string getSearchPropertyStringValue() const;
    [[nodiscard]] int getSearchPropertyIntValue() const;

    [[nodiscard]] std::string getUpdateProperty() const;
    [[nodiscard]] bool getUpdatePropertyStringValue(std::string& value) const;
    [[nodiscard]] bool getUpdatePropertyIntValue(int& value) const;

    // 判断 KEY 是否是 user_profile 字段
    static bool isProperty(const std::string& value);
};

inline UserBaseInfo UserBaseInfo::fromResult(const std::shared_ptr<sql::ResultSet> &result) {
    UserBaseInfo info;
    info.fromSqlResult(result);
    return info;
}

inline void UserBaseInfo::fromJson(Json::Value &value) {
    if (value.isMember("id") && !value["id"].isNull()) {
        id = value["id"].asInt();
    }
    if (value.isMember("uid") && !value["uid"].isNull()) {
        uid = std::stoi(value["uid"].asString());
    }
    if (value.isMember("gender") && !value["gender"].isNull()) {
        gender = static_cast<int8_t>(value["gender"].asInt());
    }
    if (value.isMember("name") && !value["name"].isNull()) {
        name = value["name"].asString();
    }
    if (value.isMember("email") && !value["email"].isNull()) {
        email = value["email"].asString();
    }
    if (value.isMember("password") && !value["password"].isNull()) {
        password = value["password"].asString();
    }
    if (value.isMember("salt") && !value["salt"].isNull()) {
        salt = value["salt"].asString();
    }
    if (value.isMember("avatar_url") && !value["avatar_url"].isNull()) {
        avatarUrl = value["avatar_url"].asString();
    }
    if (value.isMember("create_time") && !value["create_time"].isNull()) {
        createTime = value["create_time"].asString();
    }
    if (value.isMember("update_time") && !value["update_time"].isNull()) {
        updateTime = value["update_time"].asString();
    }
}

inline void UserBaseInfo::fromSqlResult(const std::shared_ptr<sql::ResultSet> &result) {
    id = result->getInt("id");
    uid = result->getInt("uid");
    gender = static_cast<int8_t>(result->getInt("gender"));
    name = result->getString("name");
    email = result->getString("email");
    avatarUrl = result->getString("avatar");
    createTime = result->getString("create_time");
    updateTime = result->getString("update_time");
}

inline void UserBaseInfo::toJson(Json::Value &value) const {
    if (uid < 0) {
        throw std::invalid_argument("invalid uid");
    }

    value["uid"] = uid;
    value["id"] = id;
    if (name.has_value()) {
        value["name"] = name.value();
    }
    if (email.has_value()) {
        value["email"] = email.value();
    }
    if (gender == static_cast<int8_t>(GenderType::MALE)
        || gender == static_cast<int8_t>(GenderType::FEMALE)) {
        value["gender"] = gender;
    }
    if (password.has_value()) {
        value["password"] = password.value();
    }
    if (salt.has_value()) {
        value["salt"] = salt.value();
    }
    if (avatarUrl.has_value()) {
        value["avatar_url"] = avatarUrl.value();
    }
    if (createTime.has_value()) {
        value["create_time"] = createTime.value();
    }
    if (updateTime.has_value()) {
        value["update_time"] = updateTime.value();
    }
}

inline std::string UserBaseInfo::getSearchProperty() const {
    // 优先使用 UID、邮箱搜索
    if (uid >= 0) {
        return "uid";
    }
    if (email.has_value()) {
        return "email";
    }
    if (name.has_value()) {
        return "name";
    }
    return "";
}

inline std::string UserBaseInfo::getSearchPropertyStringValue() const {
    if (email.has_value()) {
        return email.value();
    }
    if (name.has_value()) {
        return name.value();
    }
    return "";
}

inline int UserBaseInfo::getSearchPropertyIntValue() const {
    if (uid < 0) {
        return -1;
    }
    return uid;
}

inline std::string UserBaseInfo::getUpdateProperty() const {
    if (gender == static_cast<int8_t>(GenderType::MALE)
        || gender == static_cast<int8_t>(GenderType::FEMALE)) {
        return "gender";
    }
    if (name.has_value()) {
        return "name";
    }
    if (email.has_value()) {
        return "email";
    }
    if (password.has_value()) {
        return "password";
    }
    if (salt.has_value()) {
        return "salt";
    }
    if (avatarUrl.has_value()) {
        return "avatar_url";
    }
    if (createTime.has_value()) {
        return "create_time";
    }
    if (updateTime.has_value()) {
        return "update_time";
    }
    return "";
}

inline bool UserBaseInfo::getUpdatePropertyStringValue(std::string &value) const {
    if (name.has_value()) {
        value = name.value();
        return true;
    }
    if (email.has_value()) {
        value = email.value();
        return true;
    }
    if (password.has_value()) {
        value = password.value();
        return true;
    }
    if (salt.has_value()) {
        value = salt.value();
        return true;
    }
    if (avatarUrl.has_value()) {
        value = avatarUrl.value();
        return true;
    }
    if (createTime.has_value()) {
        value = createTime.value();
        return true;
    }
    if (updateTime.has_value()) {
        value = updateTime.value();
        return true;
    }
    return false;
}

inline bool UserBaseInfo::getUpdatePropertyIntValue(int &value) const {
    if (gender == static_cast<int8_t>(GenderType::MALE) || gender == static_cast<int8_t>(GenderType::FEMALE)) {
        value = gender;
        return true;
    }
    return false;
}

inline bool UserBaseInfo::isProperty(const std::string &value) {
    return keys.find(value) != keys.end();
}

#endif //IMSERVER_USERBASEINFO_H
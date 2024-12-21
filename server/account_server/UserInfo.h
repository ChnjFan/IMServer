//
// Created by fan on 24-12-18.
//

#ifndef IMSERVER_USERINFO_H
#define IMSERVER_USERINFO_H

#include <string>
#include "Message.h"
#include "IM.AccountServer.pb.h"

class UserInfo;
typedef std::shared_ptr<UserInfo> UserInfoPtr;

class UserInfo {
public:
    UserInfo() = default;

    static UserInfoPtr getUserInfo(const IM::Account::ImMsgUserStatus& status);

    const std::string &getAccid() const;

    void setAccid(const std::string &accid);

    const std::string &getToken() const;

    void setToken(const std::string &token);

    const std::string &getEmail() const;

    void setEmail(const std::string &email);

    const std::string &getMobile() const;

    void setMobile(const std::string &mobile);

    const std::string &getPassword() const;

    void setPassword(const std::string &password);

    const std::string &getName() const;

    void setName(const std::string &name);

    const std::string &getIcon() const;

    void setIcon(const std::string &icon);

    const std::string &getSign() const;

    void setSign(const std::string &sign);

    int32_t getGender() const;

    void setGender(int32_t gender);

    const std::string &getEx() const;

    void setEx(const std::string &ex);

private:
    std::string accid;
    std::string token;
    std::string email;
    std::string mobile;
    std::string password;
    std::string name;
    std::string icon;
    std::string sign;
    int32_t gender;
    std::string ex;
};


#endif //IMSERVER_USERINFO_H

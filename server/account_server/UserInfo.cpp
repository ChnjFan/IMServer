//
// Created by fan on 24-12-18.
//

#include "UserInfo.h"
#include "IM.AccountServer.pb.h"
#include "Exception.h"

UserInfoPtr UserInfo::getUserInfo(const IM::Account::ImMsgUserStatus& status) {
    UserInfoPtr pUserInfo = std::make_shared<UserInfo>();
    pUserInfo->setAccid(status.acc_id());
    if (status.has_token())
        pUserInfo->setToken(status.token());
    if (status.has_email())
        pUserInfo->setEmail(status.email());
    if (status.has_mobile())
        pUserInfo->setMobile(status.mobile());
    if (status.has_password())
        pUserInfo->setPassword(status.password());
    if (status.has_name())
        pUserInfo->setName(status.name());
    if (status.has_icon())
        pUserInfo->setIcon(status.icon());
    if (status.has_sign())
        pUserInfo->setGender(status.gender());
    if (status.has_ex())
        pUserInfo->setEx(status.ex());
    return pUserInfo;
}

UserInfoPtr UserInfo::getUserInfo(const IM::Account::ImMsgLoginReq &loginInfo) {
    UserInfoPtr pUserInfo = std::make_shared<UserInfo>();
    pUserInfo->setPassword(loginInfo.password());
    if (loginInfo.has_acc_id())
        pUserInfo->setAccid(loginInfo.acc_id());
    if (loginInfo.has_email())
        pUserInfo->setEmail(loginInfo.email());
    if (loginInfo.has_mobile())
        pUserInfo->setMobile(loginInfo.mobile());
    if (loginInfo.has_token())
        pUserInfo->setToken(loginInfo.token());
    return pUserInfo;
}

const std::string &UserInfo::getAccid() const {
    return accid;
}

void UserInfo::setAccid(const std::string &accid) {
    UserInfo::accid = accid;
}

const std::string &UserInfo::getToken() const {
    return token;
}

void UserInfo::setToken(const std::string &token) {
    UserInfo::token = token;
}

const std::string &UserInfo::getEmail() const {
    return email;
}

void UserInfo::setEmail(const std::string &email) {
    UserInfo::email = email;
}

const std::string &UserInfo::getMobile() const {
    return mobile;
}

void UserInfo::setMobile(const std::string &mobile) {
    UserInfo::mobile = mobile;
}

const std::string &UserInfo::getPassword() const {
    return password;
}

void UserInfo::setPassword(const std::string &password) {
    UserInfo::password = password;
}

const std::string &UserInfo::getName() const {
    return name;
}

void UserInfo::setName(const std::string &name) {
    UserInfo::name = name;
}

const std::string &UserInfo::getIcon() const {
    return icon;
}

void UserInfo::setIcon(const std::string &icon) {
    UserInfo::icon = icon;
}

const std::string &UserInfo::getSign() const {
    return sign;
}

void UserInfo::setSign(const std::string &sign) {
    UserInfo::sign = sign;
}

int32_t UserInfo::getGender() const {
    return gender;
}

void UserInfo::setGender(int32_t gender) {
    UserInfo::gender = gender;
}

const std::string &UserInfo::getEx() const {
    return ex;
}

void UserInfo::setEx(const std::string &ex) {
    UserInfo::ex = ex;
}

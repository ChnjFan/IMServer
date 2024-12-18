//
// Created by fan on 24-12-18.
//

#include "UserInfo.h"
#include "IM.AccountServer.pb.h"
#include "Exception.h"

UserInfoPtr UserInfo::getUserInfo(Base::Message &message) {
    if (message.getTypeName() != "IM::Account::ImMsgUserStatus")
        return nullptr;

    std::shared_ptr<IM::Account::ImMsgUserStatus> pUserStatus =
            std::dynamic_pointer_cast<IM::Account::ImMsgUserStatus>(message.deserialize());
    if (!pUserStatus)
        return nullptr;

    UserInfoPtr pUserInfo = std::make_shared<UserInfo>();
    pUserInfo->setAccid(pUserStatus->accid());
    if (pUserStatus->has_token())
        pUserInfo->setToken(pUserStatus->token());
    if (pUserStatus->has_email())
        pUserInfo->setEmail(pUserStatus->email());
    if (pUserStatus->has_mobile())
        pUserInfo->setMobile(pUserStatus->mobile());
    if (pUserStatus->has_password())
        pUserInfo->setPassword(pUserStatus->password());
    if (pUserStatus->has_name())
        pUserInfo->setName(pUserStatus->name());
    if (pUserStatus->has_icon())
        pUserInfo->setIcon(pUserStatus->icon());
    if (pUserStatus->has_sign())
        pUserInfo->setGender(pUserStatus->gender());
    if (pUserStatus->has_ex())
        pUserInfo->setEx(pUserStatus->ex());
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

//
// Created by Fan on 2026/5/12.
//

#ifndef IMSERVER_MSGNODE_H
#define IMSERVER_MSGNODE_H

#include <string>
#include <iostream>
#include <memory>

class MsgNode {
public:
    explicit MsgNode(uint16_t capacity);
    ~MsgNode();

    void clear() const;

    uint16_t used_;
    uint16_t capacity_;
    char *buffer_;
};

class RecvNode : public MsgNode {
public:
    RecvNode(uint16_t size, uint16_t msgId);
    uint16_t msgId_;
};

class SendNode : public MsgNode {
public:
    SendNode(const char* msg, uint16_t size, uint16_t msgId);
    uint16_t msgId_;
};

class Session;

class LogicNode : public RecvNode {
public:
    LogicNode(const std::shared_ptr<Session> &session, const RecvNode &node);
    std::shared_ptr<Session> session_;
};


#endif //IMSERVER_MSGNODE_H
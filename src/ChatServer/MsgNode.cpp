//
// Created by Fan on 2026/5/12.
//

#include <boost/asio.hpp>
#include <utility>

#include "const.h"
#include "MsgNode.h"

MsgNode::MsgNode(const uint16_t capacity) : used_(0), capacity_(capacity) {
    buffer_ = new char[capacity+1];
}

MsgNode::~MsgNode() {
    delete[] buffer_;
}

void MsgNode::clear() const {
    if (buffer_ == nullptr) return;
    memset(buffer_, 0, capacity_+1);
}

RecvNode::RecvNode(const uint16_t size, const uint16_t msgId) : MsgNode(size), msgId_(msgId) {
}

SendNode::SendNode(const char *msg, const uint16_t size, const uint16_t msgId)
    : MsgNode(size+HEAD_TOTAL_LEN+1), msgId_(msgId) {
    const uint16_t msgIdHost = net::detail::socket_ops::host_to_network_short(msgId);
    memcpy(buffer_, &msgIdHost, HEAD_MSG_ID_LEN);
    const uint16_t msgSizeHost = net::detail::socket_ops::host_to_network_short(size);
    memcpy(buffer_ + HEAD_MSG_ID_LEN, &msgSizeHost, HEAD_MSG_SIZE_LEN);
    memcpy(buffer_ + HEAD_TOTAL_LEN, msg, size);
    used_ = size + HEAD_TOTAL_LEN;
}

LogicNode::LogicNode(const std::shared_ptr<Session> &session, const RecvNode &node) : RecvNode(node) {
    session_ = session;
}

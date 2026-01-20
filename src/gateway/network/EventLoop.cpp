#include "EventLoop.h"

namespace network {

EventLoop::EventLoop() 
    : work_(std::make_unique<boost::asio::io_context::work>(io_context_)),
      running_(false) {
}

EventLoop::~EventLoop() {
    stop();
}

void EventLoop::start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread([this]() {
            io_context_.run();
        });
    }
}

void EventLoop::stop() {
    if (running_) {
        running_ = false;
        work_.reset();
        io_context_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

boost::asio::io_context& EventLoop::getIoContext() {
    return io_context_;
}

bool EventLoop::isRunning() const {
    return running_;
}

} // namespace network

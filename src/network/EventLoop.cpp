#include "EventLoop.h"

namespace network {

/**
 * @brief Initialize an EventLoop and keep its io_context ready to run.
 *
 * Constructs the EventLoop, creates a work guard that prevents the internal
 * io_context from stopping while idle, and sets the running flag to false.
 */
EventLoop::EventLoop() 
    : work_(std::make_unique<boost::asio::io_context::work>(io_context_)),
      running_(false) {
}

/**
 * @brief Cleans up the event loop and releases its resources.
 *
 * Ensures the loop is stopped, the io_context work guard is released, and the internal
 * thread is joined so no background processing remains after destruction.
 */
EventLoop::~EventLoop() {
    stop();
}

/**
 * @brief Starts the event loop in a dedicated background thread if it is not already running.
 *
 * Sets the loop state to running and launches a thread that executes the internal io_context.
 * Has no effect if the event loop is already running.
 */
void EventLoop::start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread([this]() {
            io_context_.run();
        });
    }
}

/**
 * @brief Stops the event loop and waits for the worker thread to finish.
 *
 * If the loop is running, releases the work guard, stops the internal
 * io_context, and joins the worker thread. If the loop is not running,
 * the call has no effect.
 */
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

/**
 * @brief Provides access to the EventLoop's internal Boost.Asio io_context.
 *
 * @return boost::asio::io_context& Reference to the internal io_context used to run asynchronous operations.
 */
boost::asio::io_context& EventLoop::getIoContext() {
    return io_context_;
}

/**
 * @brief Indicates whether the event loop is currently running.
 *
 * @return true if the event loop is active, false otherwise.
 */
bool EventLoop::isRunning() const {
    return running_;
}

} // namespace network
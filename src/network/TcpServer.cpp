#include "TcpServer.h"
#include <iostream>

namespace network {

/**
 * @brief Constructs a TcpServer bound to the given address and port using the provided IO context.
 *
 * Initializes the server's acceptor to listen on the specified IP address and port and sets the server
 * to the stopped state.
 *
 * @param io_context IO context used for asynchronous operations.
 * @param address IP address to bind the server to (text form, e.g. "0.0.0.0" or "::1").
 * @param port TCP port to bind the server to.
 */
TcpServer::TcpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port)),
      running_(false) {
}

/**
 * @brief Shut down the server and release network resources.
 *
 * Stops accepting new connections, closes all active connections, and releases
 * underlying acceptor and socket resources so the object can be safely destroyed.
 */
TcpServer::~TcpServer() {
    stop();
}

/**
 * @brief Begins accepting incoming TCP connections if the server is not already running.
 *
 * If the server is stopped, sets the running state to true and starts the asynchronous accept loop.
 */
void TcpServer::start() {
    if (!running_) {
        running_ = true;
        doAccept();
    }
}

/**
 * @brief Stops the server and closes all active connections.
 *
 * Stops accepting new connections, closes the acceptor, closes each active Connection,
 * clears the tracked connection set, and marks the server as not running.
 */
void TcpServer::stop() {
    if (running_) {
        running_ = false;
        boost::system::error_code ec;
        acceptor_.close(ec);
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            conn->close();
        }
        connections_.clear();
    }
}

/**
 * @brief Sets the callback invoked when a connection receives a message.
 *
 * Stores the provided handler and applies it to subsequently accepted connections.
 *
 * @param handler Function called for each message received on a connection.
 */
void TcpServer::setMessageHandler(Connection::MessageHandler handler) {
    message_handler_ = handler;
}

/**
 * @brief Sets the callback invoked when a connection is closed.
 *
 * @param handler Function to call with the closed connection pointer when a connection is removed.
 */
void TcpServer::setCloseHandler(Connection::CloseHandler handler) {
    close_handler_ = handler;
}

/**
 * @brief Reports whether the server is currently running.
 *
 * @return `true` if the server is running and accepting new connections, `false` otherwise.
 */
bool TcpServer::isRunning() const {
    return running_;
}

/**
 * @brief Start or continue the asynchronous accept loop on the server acceptor.
 *
 * Schedules an asynchronous accept operation that, on a successful accept while the server
 * is running, forwards the new socket to the connection handler and re-arms the accept loop.
 */
void TcpServer::doAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec && running_) {
                handleAccept(ec, std::move(socket));
                doAccept();
            }
        });
}

/**
 * @brief Handle completion of an asynchronous accept and initialize the new connection.
 *
 * Creates a Connection from the accepted socket when the accept completed successfully,
 * installs the server's message and close handlers, adds the connection to the server's
 * active set, and starts its I/O processing.
 *
 * @param ec Error code resulting from the accept operation; no action is taken if an error occurred.
 * @param socket The accepted TCP socket (moved into the new Connection on success).
 */
void TcpServer::handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (!ec) {
        auto conn = std::make_shared<Connection>(std::move(socket));
        conn->setMessageHandler(message_handler_);
        
        auto self = shared_from_this();
        conn->setCloseHandler([this, self](Connection::Ptr conn) {
            removeConnection(conn);
            if (close_handler_) {
                close_handler_(conn);
            }
        });
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.insert(conn);
        }
        
        conn->start();
    }
}

/**
 * @brief Removes a connection from the server's active connection set.
 *
 * Removes the specified connection from the server's tracked connections in a thread-safe manner.
 *
 * @param conn Shared pointer to the Connection to remove. If the connection is not present, no action is taken.
 */
void TcpServer::removeConnection(Connection::Ptr conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn);
}

} // namespace network
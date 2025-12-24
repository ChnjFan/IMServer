#include "Connection.h"
#include <iostream>

namespace network {

/**
 * @brief Constructs a Connection by taking ownership of a connected TCP socket.
 *
 * Takes ownership of the provided socket (moved) and prepares internal state:
 * initializes the receive buffer to 1024 bytes and marks the connection as open.
 *
 * @param socket TCP socket to be owned by the Connection (moved).
 */
Connection::Connection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket)),
      read_buffer_(1024),
      closed_(false) {
}

/**
 * @brief Ensures the connection is closed when the object is destroyed.
 *
 * Calls close() to shut down the socket and release resources; any registered close handler will be invoked.
 */
Connection::~Connection() {
    close();
}

/**
 * @brief Begins the connection's read loop to receive incoming data.
 *
 * Starts continuous read operations on the underlying socket so received bytes
 * are delivered to the registered message handler.
 */
void Connection::start() {
    doRead();
}

/**
 * @brief Closes the connection and invokes the registered close handler.
 *
 * Marks the connection as closed, closes the underlying socket, and if a close handler
 * is registered invokes it with a shared_ptr to this Connection. Calling this method
 * when the connection is already closed has no effect.
 */
void Connection::close() {
    if (!closed_) {
        closed_ = true;
        boost::system::error_code ec;
        socket_.close(ec);
        if (close_handler_) {
            close_handler_(shared_from_this());
        }
    }
}

/**
 * @brief Appends bytes to the connection's outgoing buffer and starts sending if idle.
 *
 * Appends the contents of `data` to the connection's outgoing buffer. If the connection is closed, the call has no effect. If there is no ongoing send operation, an asynchronous write is initiated to transmit the buffered data. This method is safe to call concurrently from multiple threads.
 *
 * @param data Byte sequence to send; copied into the connection's outgoing buffer.
 */
void Connection::send(const std::vector<char>& data) {
    if (closed_) return;
    
    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_buffer_.empty();
        write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
    }
    
    if (!write_in_progress) {
        doWrite();
    }
}

/**
 * @brief Obtains the remote TCP endpoint for this connection.
 *
 * If the socket's remote endpoint cannot be retrieved, returns a default-constructed
 * endpoint.
 *
 * @return boost::asio::ip::tcp::endpoint The remote endpoint, or a default-constructed
 * endpoint if an error occurred while querying the socket.
 */
boost::asio::ip::tcp::endpoint Connection::getRemoteEndpoint() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return boost::asio::ip::tcp::endpoint();
    }
    return endpoint;
}

/**
 * @brief Register a callback invoked when a message is received on this connection.
 *
 * @param handler Callable that will be invoked for each incoming message with
 *        the connection (`std::shared_ptr<Connection>`) and the message payload (`std::vector<char>`).
 */
void Connection::setMessageHandler(MessageHandler handler) {
    message_handler_ = handler;
}

/**
 * @brief Registers a callback invoked when the connection is closed.
 *
 * The handler will be called with a shared_ptr to this Connection when the connection transitions to the closed state.
 *
 * @param handler Callable invoked on close; receives a std::shared_ptr<Connection> representing the closed connection.
 */
void Connection::setCloseHandler(CloseHandler handler) {
    close_handler_ = handler;
}

/**
 * @brief Checks whether the connection has been closed.
 *
 * @return `true` if the connection is closed, `false` otherwise.
 */
bool Connection::isClosed() const {
    return closed_;
}

/**
 * @brief Continues reading incoming bytes from the socket and dispatches each received chunk.
 *
 * This method runs a continuous read loop that delivers every received buffer slice to the
 * registered message handler. If a read error occurs, the connection is closed.
 */
void Connection::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::vector<char> data(read_buffer_.begin(), read_buffer_.begin() + length);
                if (message_handler_) {
                    message_handler_(self, data);
                }
                doRead();
            } else {
                close();
            }
        });
}

/**
 * @brief Initiates an asynchronous write of the connection's pending send buffer.
 *
 * Starts an async write of the current contents of `write_buffer_`, holding a
 * shared pointer to the connection for the duration of the operation. On
 * successful completion the pending buffer is cleared while holding
 * `write_mutex_`; on error the connection is closed.
 */
void Connection::doWrite() {
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(write_buffer_),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_buffer_.clear();
            } else {
                close();
            }
        });
}

} // namespace network
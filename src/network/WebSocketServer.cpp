#include "WebSocketServer.h"
#include <iostream>
#include <regex>
#include <string>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace network {

/**
 * @brief Construct a WebSocketServer bound to the given address and port using the provided io_context.
 *
 * Initializes the server's IO context and TCP acceptor so the instance is ready to start accepting connections.
 *
 * @param io_context Reference to the Boost.Asio IO context used for asynchronous operations.
 * @param address IP address or interface to bind the acceptor to (e.g., "0.0.0.0" or "127.0.0.1").
 * @param port TCP port to listen on.
 */
WebSocketServer::WebSocketServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port)),
      running_(false) {
}

/**
 * @brief Cleans up the WebSocketServer by stopping it and closing the acceptor and all active connections.
 *
 * Ensures the server is shut down and resources used for accepting and managing connections are released.
 */
WebSocketServer::~WebSocketServer() {
    stop();
}

/**
 * @brief Starts the WebSocket server accept loop.
 *
 * If the server is not already running, marks it as running and begins accepting incoming TCP connections.
 * Has no effect if the server is already running.
 */
void WebSocketServer::start() {
    if (!running_) {
        running_ = true;
        doAccept();
    }
}

/**
 * @brief Stops the WebSocket server and closes all active connections.
 *
 * Sets the running state to false, closes the listening acceptor (errors ignored),
 * and closes and clears all active connections. The connections set is modified
 * under its mutex to ensure thread-safe shutdown.
 */
void WebSocketServer::stop() {
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
 * @brief Install the callback to be invoked when a complete WebSocket message payload is received.
 *
 * @param handler Callable that will be called for each complete message; it will be provided the originating connection and the message payload.
 */
void WebSocketServer::setMessageHandler(Connection::MessageHandler handler) {
    message_handler_ = handler;
}

/**
 * @brief Installs a callback to be invoked when a connection is closed.
 *
 * The provided handler will be called whenever a connection managed by this
 * server transitions to a closed state.
 *
 * @param handler Callable invoked on connection close; it receives the closed connection as its argument.
 */
void WebSocketServer::setCloseHandler(Connection::CloseHandler handler) {
    close_handler_ = handler;
}

/**
 * @brief Indicates whether the WebSocket server is currently running.
 *
 * @return `true` if the server is running and accepting connections, `false` otherwise.
 */
bool WebSocketServer::isRunning() const {
    return running_;
}

/**
 * @brief Starts an asynchronous accept loop that listens for new TCP connections.
 *
 * Initiates a single asynchronous accept operation; when a connection is accepted successfully and the server
 * is currently running, the accepted socket is passed to handleAccept() and another accept is scheduled to
 * continue the loop.
 */
void WebSocketServer::doAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec && running_) {
                handleAccept(ec, std::move(socket));
                doAccept();
            }
        });
}

/**
 * @brief Handle completion of an asynchronous accept operation.
 *
 * On successful accept, creates a Connection from the accepted socket, configures
 * an initial message handler to perform the WebSocket handshake, sets a close
 * handler that removes the connection and invokes the user-provided close
 * callback (if any), stores the connection in the server's connection set
 * (protected by a mutex), and starts the connection.
 *
 * @param ec The result of the accept operation; when not an error, the socket is used.
 * @param socket The accepted TCP socket; ownership is moved into the new Connection.
 */
void WebSocketServer::handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (!ec) {
        auto conn = std::make_shared<Connection>(std::move(socket));
        
        // 首先处理WebSocket握手
        auto self = shared_from_this();
        conn->setMessageHandler([this, self, conn](Connection::Ptr c, const std::vector<char>& data) {
            handleWebSocketHandshake(c, data);
        });
        
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
 * @brief Remove a connection from the server's set of active connections.
 *
 * Performs a thread-safe removal of the provided connection pointer from the internal
 * connections set. This does not close or otherwise modify the connection itself.
 *
 * @param conn Shared pointer to the Connection to remove.
 */
void WebSocketServer::removeConnection(Connection::Ptr conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn);
}

/**
 * @brief Encodes binary data to Base64 without inserting newline characters.
 *
 * @param data Pointer to the input bytes to encode.
 * @param len Number of bytes pointed to by `data`.
 * @return std::string Base64-encoded representation of the input data.
 */
std::string base64_encode(const unsigned char* data, size_t len) {
    // 使用OpenSSL的BIO方法进行Base64编码
    BIO* bio = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    bio = BIO_push(bio, bmem);
    
    // 关闭换行符添加
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    
    // 写入数据并刷新
    BIO_write(bio, data, len);
    BIO_flush(bio);
    
    // 读取编码后的数据
    BUF_MEM* bptr;
    BIO_get_mem_ptr(bio, &bptr);
    
    // 创建结果字符串
    std::string encoded(bptr->data, bptr->data + bptr->length);
    
    // 释放资源
    BIO_free_all(bio);
    
    return encoded;
}

/**
 * @brief Processes an incoming HTTP WebSocket handshake and upgrades the connection to WebSocket.
 *
 * Extracts the `Sec-WebSocket-Key` from the provided HTTP request bytes, computes the
 * `Sec-WebSocket-Accept` value, sends the HTTP 101 Switching Protocols response, and
 * reconfigures the connection to handle WebSocket frames.
 *
 * @param conn Connection to operate on.
 * @param data Raw bytes of the HTTP handshake request.
 *
 * If the `Sec-WebSocket-Key` header is not present or cannot be parsed, the connection is closed.
 */
void WebSocketServer::handleWebSocketHandshake(Connection::Ptr conn, const std::vector<char>& data) {
    std::string request(data.begin(), data.end());
    
    // 解析Sec-WebSocket-Key
    std::regex key_regex("Sec-WebSocket-Key: ([^\n]+)");
    std::smatch key_match;
    if (!std::regex_search(request, key_match, key_regex)) {
        conn->close();
        return;
    }
    
    std::string key = key_match[1].str();
    const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic_string;
    
    // SHA-1哈希
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), sha1_hash);
    
    // Base64编码
    std::string response_key = base64_encode(sha1_hash, SHA_DIGEST_LENGTH);
    
    // 构建响应
    std::stringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << response_key << "\r\n";
    response << "\r\n";
    
    // 发送响应
    std::string response_str = response.str();
    conn->send(std::vector<char>(response_str.begin(), response_str.end()));
    
    // 切换到WebSocket数据帧处理
    auto self = shared_from_this();
    conn->setMessageHandler([this, self](Connection::Ptr c, const std::vector<char>& data) {
        handleWebSocketFrame(c, data);
    });
}

/**
 * @brief Parse a single WebSocket frame from a raw buffer and handle it.
 *
 * Parses the provided buffer as one WebSocket frame (FIN, opcode, payload length,
 * extended lengths, and masking). If the buffer does not contain a complete frame
 * the function returns without action. For text (opcode 0x01) and binary (opcode 0x02)
 * frames the decoded payload is forwarded to the configured message handler. For a
 * close frame (opcode 0x08) the connection is closed.
 *
 * @param conn Shared pointer to the connection that sent the frame.
 * @param data Raw bytes containing the WebSocket frame to parse and process.
 */
void WebSocketServer::handleWebSocketFrame(Connection::Ptr conn, const std::vector<char>& data) {
    if (data.size() < 2) return;
    
    // 解析WebSocket帧头
    bool fin = (data[0] & 0x80) != 0;
    bool mask = (data[1] & 0x80) != 0;
    uint8_t opcode = data[0] & 0x0F;
    uint64_t payload_length = data[1] & 0x7F;
    
    size_t header_offset = 2;
    
    // 处理扩展长度
    if (payload_length == 126) {
        if (data.size() < 4) return;
        payload_length = (static_cast<uint64_t>(data[2]) << 8) | data[3];
        header_offset = 4;
    } else if (payload_length == 127) {
        if (data.size() < 10) return;
        payload_length = 0;
        for (int i = 0; i < 8; i++) {
            payload_length = (payload_length << 8) | data[header_offset + i];
        }
        header_offset = 10;
    }
    
    // 处理掩码
    std::vector<uint8_t> masking_key;
    if (mask) {
        if (data.size() < header_offset + 4) return;
        masking_key = std::vector<uint8_t>(data.begin() + header_offset, data.begin() + header_offset + 4);
        header_offset += 4;
    }
    
    // 提取有效载荷
    if (data.size() < header_offset + payload_length) return;
    std::vector<char> payload(data.begin() + header_offset, data.begin() + header_offset + payload_length);
    
    // 解掩码
    if (mask) {
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] ^= masking_key[i % 4];
        }
    }
    
    // 处理不同类型的帧
    if (opcode == 0x01) { // 文本帧
        if (message_handler_) {
            message_handler_(conn, payload);
        }
    } else if (opcode == 0x02) { // 二进制帧
        if (message_handler_) {
            message_handler_(conn, payload);
        }
    } else if (opcode == 0x08) { // 关闭帧
        conn->close();
    }
    // 其他帧类型可以根据需要扩展
}

} // namespace network
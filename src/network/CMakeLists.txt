# network module CMakeLists.txt

# 设置源文件
set(SOURCES
    EventLoop.cpp
    Connection.cpp
    TcpServer.cpp
    WebSocketServer.cpp
)

# 设置头文件
set(HEADERS
    EventLoop.h
    Connection.h
    TcpServer.h
    WebSocketServer.h
)

# 创建静态库
add_library(network STATIC ${SOURCES} ${HEADERS})

# 添加头文件搜索路径
target_include_directories(network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 链接Boost库
target_link_libraries(network PRIVATE Boost::asio Boost::system Boost::thread)

# 链接OpenSSL库（用于WebSocket的SHA-1和Base64编码）
target_link_libraries(network PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# 设置C++标准
target_compile_features(network PRIVATE cxx_std_17)

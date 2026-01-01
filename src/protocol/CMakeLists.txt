# Protocol Layer CMakeLists.txt

# 协议层源文件列表
set(PROTOCOL_SOURCES
    ProtocolManager.cpp
    Parser.cpp
    MessageRouter.cpp
    AsyncExecutor.cpp
    Message.cpp
)

# 协议层头文件列表
set(PROTOCOL_HEADERS
    ProtocolManager.h
    Parser.h
    MessageRouter.h
    AsyncExecutor.h
    Message.h
)

# 创建协议层静态库
add_library(protocol STATIC
    ${PROTOCOL_SOURCES}
    ${PROTOCOL_HEADERS}
)

# 设置包含目录
target_include_directories(protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/network
    ${BOOST_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(protocol
    network
    Boost::asio
    Boost::system
    Boost::beast
    ${CMAKE_THREAD_LIBS_INIT}
)

# 设置C++标准
target_compile_features(protocol PUBLIC cxx_std_17)

# 添加编译选项
if(MSVC)
    target_compile_options(protocol PRIVATE /W4 /WX /wd4251)
else()
    target_compile_options(protocol PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

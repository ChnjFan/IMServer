# network模块的CMakeLists.txt配置

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Boost库
find_package(Boost REQUIRED COMPONENTS system thread)

# 收集源文件
file(GLOB_RECURSE NETWORK_SOURCES "*.cpp")
file(GLOB_RECURSE NETWORK_HEADERS "*.h")

# 创建可执行文件（作为单独进程）
add_executable(network_module ${NETWORK_SOURCES} ${NETWORK_HEADERS})

# 包含头文件目录
target_include_directories(network_module PUBLIC ${Boost_INCLUDE_DIRS})

# 链接Boost库
target_link_libraries(network_module ${Boost_LIBRARIES})

# 添加编译选项
if(MSVC)
    target_compile_options(network_module PRIVATE /W4 /WX)
else()
    target_compile_options(network_module PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# 安装配置
install(TARGETS network_module DESTINATION bin)

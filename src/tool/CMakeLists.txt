# 工具模块的CMakeLists.txt

# 查找必要的包
find_package(Threads REQUIRED)

# 设置工具模块头文件
set(TOOL_HEADERS
    IdGenerator.h
)

# 设置工具模块实现文件
set(TOOL_SOURCES
    IdGenerator.cpp
)

# 创建工具模块库
add_library(tool_module STATIC ${TOOL_HEADERS} ${TOOL_SOURCES})

# 设置目标链接库
target_link_libraries(tool_module 
    Threads::Threads
)

# 设置包含目录
target_include_directories(tool_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置C++标准
target_compile_features(tool_module PUBLIC cxx_std_17)

# 设置编译选项
if(MSVC)
    target_compile_options(tool_module PRIVATE /W4 /WX)
else()
    target_compile_options(tool_module PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

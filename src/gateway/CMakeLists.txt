# Gateway Layer CMakeLists.txt

# 定义目标名称
set(TARGET_NAME gateway)

# 收集源文件
file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

# 创建库
add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS})

# 设置编译选项
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
target_compile_options(${TARGET_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /MP>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -pedantic>
)

# 添加子目录
add_subdirectory(network)
add_subdirectory(protocol)

# 添加包含目录
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/tool
    ${Boost_INCLUDE_DIRS}
)

# 链接依赖
target_link_libraries(${TARGET_NAME} PUBLIC
    gateway_network
    gateway_protocol
    tool_module
    ${Boost_LIBRARIES}
    $<$<CXX_COMPILER_ID:MSVC>:ws2_32>
    $<$<CXX_COMPILER_ID:MSVC>:mswsock>
    $<$<CXX_COMPILER_ID:MSVC>:advapi32>
)

# 添加示例可执行文件
add_executable(gateway_example main.cpp)
target_link_libraries(gateway_example PRIVATE ${TARGET_NAME})

# 安装目标
install(TARGETS ${TARGET_NAME} EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 安装头文件
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gateway)

# 安装示例可执行文件
install(TARGETS gateway_example DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)

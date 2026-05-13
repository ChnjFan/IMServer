cmake_minimum_required(VERSION 3.15)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

project(IMServer
    VERSION 1.0.0
    DESCRIPTION "Instant Messaging Server Suite"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GATE_SERVER "Build GateServer" ON)
option(BUILD_CHAT_SERVER "Build ChatServer" ON)
option(BUILD_STATUS_SERVER "Build StatusServer" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

find_package(Boost 1.90 REQUIRED COMPONENTS filesystem)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_library(jsoncpp_deps INTERFACE)
target_include_directories(jsoncpp_deps INTERFACE ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(jsoncpp_deps INTERFACE ${JSONCPP_LINK_LIBRARIES})

find_package(mysql-concpp REQUIRED CONFIG COMPONENTS jdbc)
add_library(mysqlcppconn_deps INTERFACE)
target_link_libraries(mysqlcppconn_deps INTERFACE mysql::concpp-jdbc)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

add_library(grpc_deps INTERFACE)
target_link_libraries(grpc_deps INTERFACE
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
)

pkg_check_modules(HIREDIS REQUIRED hiredis)

add_library(hiredis_deps INTERFACE)
target_include_directories(hiredis_deps INTERFACE ${HIREDIS_INCLUDE_DIRS})
target_link_libraries(hiredis_deps INTERFACE ${HIREDIS_LINK_LIBRARIES})

function(generate_grpc_code proto_files output_dir)
    set(PROTO_SRC_DIR ${PROJECT_SOURCE_DIR}/proto)
    
    foreach(proto_file ${proto_files})
        get_filename_component(proto_name ${proto_file} NAME_WE)
        
        set(generated_srcs
            ${output_dir}/${proto_name}.pb.cc
            ${output_dir}/${proto_name}.grpc.pb.cc
        )
        set(generated_hdrs
            ${output_dir}/${proto_name}.pb.h
            ${output_dir}/${proto_name}.grpc.pb.h
        )
        
        add_custom_command(
            OUTPUT ${generated_srcs} ${generated_hdrs}
            COMMAND protobuf::protoc
            ARGS 
                --grpc_out=${output_dir}
                --cpp_out=${output_dir}
                -I${PROTO_SRC_DIR}
                --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                ${PROTO_SRC_DIR}/${proto_file}
            MAIN_DEPENDENCY ${PROTO_SRC_DIR}/${proto_file}
            COMMENT "Generating gRPC code from ${proto_file}"
            VERBATIM
        )
        
        list(APPEND GENERATED_PROTO_SRCS ${generated_srcs})
        list(APPEND GENERATED_PROTO_HDRS ${generated_hdrs})
    endforeach()
    
    set(GENERATED_PROTO_SRCS ${GENERATED_PROTO_SRCS} PARENT_SCOPE)
    set(GENERATED_PROTO_HDRS ${GENERATED_PROTO_HDRS} PARENT_SCOPE)
endfunction()

add_subdirectory(src/base)
add_subdirectory(src/proto_client)

if(BUILD_GATE_SERVER)
    add_subdirectory(src/GateServer)
endif()

if(BUILD_CHAT_SERVER)
    add_subdirectory(src/ChatServer)
endif()

if(BUILD_STATUS_SERVER)
    add_subdirectory(src/StatusServer)
endif()

add_custom_target(copy_config ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini
    COMMENT "Copying config.ini to bin directory"
)

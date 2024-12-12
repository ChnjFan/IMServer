cmake_minimum_required(VERSION 3.28)
project(login_server)

include_directories(. ../protobuf/protocol ../base ./lib)

aux_source_directory(./ SRC_LIST)
aux_source_directory(./lib LIB_SRC_LIST)

add_executable(account_server ${SRC_LIST}
        ../service_proxy/service_proxy.cpp)
set_target_properties(account_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)
#add_library(account_service SHARED ${LIB_SRC_LIST})

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(account_server PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protobuf protocol base zmq)

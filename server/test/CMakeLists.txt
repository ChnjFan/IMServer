cmake_minimum_required(VERSION 3.28)
project(client_conn)

include_directories(.)

aux_source_directory(./ SRC_LIST)

add_executable(client_conn ${SRC_LIST})
set_target_properties(client_conn PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(client_conn PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON)
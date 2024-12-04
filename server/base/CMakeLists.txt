cmake_minimum_required(VERSION 3.28)
project(libcommon)

include_directories(.)
aux_source_directory(./ SRC_LIST)

add_library(base SHARED ${SRC_LIST})

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(base PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON)

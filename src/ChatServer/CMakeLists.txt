set(CHAT_SERVER_SOURCES
    main.cpp
)

add_executable(ChatServer ${CHAT_SERVER_SOURCES})

target_link_libraries(ChatServer
    jsoncpp_deps
)

target_include_directories(ChatServer
    PRIVATE ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)
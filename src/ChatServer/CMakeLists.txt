set(CHAT_SERVER_SOURCES
    main.cpp
    ChatServer.cpp
    ChatServer.h
    Session.cpp
    Session.h
    MsgNode.cpp
    MsgNode.h
    ChatLogicSystem.cpp
    ChatLogicSystem.h
)

add_executable(ChatServer ${CHAT_SERVER_SOURCES})

add_dependencies(ChatServer copy_config)

target_link_libraries(ChatServer
    proto_client
)

target_include_directories(ChatServer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)

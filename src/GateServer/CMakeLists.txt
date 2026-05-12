set(GATE_SERVER_SOURCES
    main.cpp
    GateServer.cpp
    GateServer.h
    HttpConnection.cpp
    HttpConnection.h
    LogicSystem.cpp
    LogicSystem.h
)

add_executable(GateServer ${GATE_SERVER_SOURCES})

add_dependencies(GateServer copy_config)

target_link_libraries(GateServer
    proto_client
)

target_include_directories(GateServer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS GateServer
    RUNTIME DESTINATION bin
)

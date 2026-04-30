set(GATE_SERVER_SOURCES
    main.cpp
        GateServer.cpp
        GateServer.h
        const.h
        HttpConnection.cpp
        HttpConnection.h
        Singleton.h
        LogicSystem.cpp
        LogicSystem.h
)

add_executable(GateServer ${GATE_SERVER_SOURCES})

target_link_libraries(GateServer
    jsoncpp_deps
)

target_include_directories(GateServer
    PRIVATE ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS GateServer
    RUNTIME DESTINATION bin
)
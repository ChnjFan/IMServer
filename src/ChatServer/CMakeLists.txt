set(CHAT_SERVER_SOURCES
    main.cpp

    # common 目录 - 通用层
    common/model/UserBaseInfo.h
    common/model/UserProfile.h
    common/model/FriendInfo.h

    # net 目录 - 网络层
    net/ChatServer.cpp
    net/ChatServer.h
    net/Session.cpp
    net/Session.h
    net/MsgNode.cpp
    net/MsgNode.h
    
    # core 目录 - 核心业务逻辑
    core/ChatLogicSystem.cpp
    core/ChatLogicSystem.h
    core/LogicWorker.cpp
    core/LogicWorker.h
    core/UserMgr.cpp
    core/UserMgr.h
    core/BatchWriter.cpp
    core/BatchWriter.h

    # db/mysql 目录 - 数据库访问层
    db/mysql/MysqlMgr.cpp
    db/mysql/MysqlMgr.h
    db/mysql/dao/UserInfoDao.cpp
    db/mysql/dao/UserInfoDao.h
    db/mysql/dao/FriendInfoDao.cpp
    db/mysql/dao/FriendInfoDao.h
    db/mysql/dao/ConversationDao.cpp
    db/mysql/dao/ConversationDao.h

    # db/redis 目录 - Redis 访问层
    db/cache/UserInfoCache.cpp
    db/cache/UserInfoCache.h
    db/cache/FriendCache.cpp
    db/cache/FriendCache.h

    # service 目录 - gRPC 服务实现
    service/ChatServiceImpl.cpp
    service/ChatServiceImpl.h
)

add_executable(ChatServer ${CHAT_SERVER_SOURCES})

add_dependencies(ChatServer copy_config)

target_link_libraries(ChatServer
    proto
)

target_include_directories(ChatServer
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/net
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/db/mysql
        ${CMAKE_CURRENT_SOURCE_DIR}/db/redis
        ${CMAKE_CURRENT_SOURCE_DIR}/service
)

install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)
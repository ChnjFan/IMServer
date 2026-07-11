# Prefer a system-installed gtest; fall back to FetchContent if not found.
#   brew install gtest   -> uses system version (no download)
#   otherwise            -> downloads via FetchContent
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    # For Windows: prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

enable_testing()

# Normalize target names across find_package vs FetchContent sources.
# find_package (cmake-config) may expose GTest::gtest; older FindGTest
# exposes GTest::GTest / GTest::Main. FetchContent uses GTest::gtest(_main).
if(NOT TARGET GTest::gtest)
    add_library(GTest::gtest INTERFACE IMPORTED)
    target_link_libraries(GTest::gtest INTERFACE GTest::GTest)
endif()
if(NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main INTERFACE IMPORTED)
    target_link_libraries(GTest::gtest_main INTERFACE GTest::Main GTest::gtest)
endif()

# test_framework — shared test utilities (built once, linked by all tests)
add_library(test_framework STATIC
    framework/protocol.cpp
    framework/account_manager.cpp
    framework/http_test_client.cpp
    framework/chat_test_client.cpp
)
target_include_directories(test_framework
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
target_link_libraries(test_framework
    PUBLIC proto
    PUBLIC jsoncpp_deps
    PUBLIC Boost::filesystem
)

# IMTest — single executable hosting all gtest cases
add_executable(IMTest
    framework/protocol_test.cpp
    gate/gate_integration_test.cpp
    status/status_integration_test.cpp
    chat/chat_client_test.cpp
)
target_link_libraries(IMTest
    PRIVATE test_framework
    PRIVATE GTest::gtest_main
    PRIVATE GTest::gmock
)

# Note: gtest_discover_tests disabled — pre-existing libssl.3.dylib SIP linker
# quirk causes the CMake test-discovery subprocess to abort on this machine.
# Tests run via direct invocation (./bin/IMTest --gtest_filter=...), which works.
# Re-enable if the environment resolves the runtime library path issue.

include(FetchContent)

# gtest via FetchContent — no system install needed
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# For Windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# test_framework — shared test utilities (built once, linked by all tests)
add_library(test_framework STATIC
    framework/protocol.cpp
    framework/account_manager.cpp
    framework/http_test_client.cpp
)
target_include_directories(test_framework
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
target_link_libraries(test_framework
    PUBLIC base
    PUBLIC jsoncpp_deps
    PUBLIC Boost::filesystem
)

# IMTest — single executable hosting all gtest cases
add_executable(IMTest
    framework/protocol_test.cpp
    gate/gate_integration_test.cpp
    status/status_integration_test.cpp
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

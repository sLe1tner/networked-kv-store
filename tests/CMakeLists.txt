include(FetchContent)

# Fetch GTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    test_connection.cpp
    test_protocol.cpp
    test_store.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        kv_server_lib
        kv_core
)

# Discover tests automatically
include(GoogleTest)
gtest_discover_tests(unit_tests)

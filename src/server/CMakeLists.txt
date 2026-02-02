# Create a library for the server logic
add_library(kv_server_lib STATIC
    tcp_server.cpp
    connection.cpp
    waker.cpp
)

target_include_directories(kv_server_lib
    PUBLIC
        # Anyone who links to kv_server_lib also gets these include paths
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(kv_server_lib
    PRIVATE
        kv_core
)

add_executable(kv_server main.cpp)

# Link the executable to the library we just made
target_link_libraries(kv_server
    PRIVATE
        kv_server_lib
)

add_executable(kv_server
    main.cpp
    tcp_server.cpp
    connection.cpp
    waker.cpp
)

target_link_libraries(kv_server
    PRIVATE
        kv_core
)

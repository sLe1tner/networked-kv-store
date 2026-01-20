add_executable(kv_client
    main.cpp
    kv_client.cpp
)

target_link_libraries(kv_client
    PRIVATE
        kv_core
)

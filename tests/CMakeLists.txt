add_executable(kv_store_test
    kv_store_test.cpp
)

target_link_libraries(kv_store_test
    PRIVATE
        kv_core
)

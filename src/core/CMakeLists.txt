add_library(kv_core
    kv_store.cpp
    protocol.cpp
    socket.cpp
    command_dispatcher.cpp
)

target_include_directories(kv_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(kv_core PUBLIC cxx_std_20)

target_link_libraries(kv_core
    PUBLIC
        Threads::Threads
)

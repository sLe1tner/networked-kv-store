
#include "tcp_server.hpp"
#include "connection.hpp"
#include <iostream>


/*
 * Entry point for the server executable.
 * parse CLI args
 * start server
 * block until shutdown
 */

int main(int argc, char* argv[]) {
    uint16_t port = argc > 1 ? std::stoi(argv[1]) : 12345;
    kv::TcpServer server{port};
    try {
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}


#include "tcp_server.hpp"
#include "connection.hpp"
#include <iostream>


/*
 * Entry point for the server executable.
 * parse CLI args
 * start server
 * block until shutdown
 */

int main() {
    kv::TcpServer server;
    try {
        server.start(12345);
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}


#include "tcp_server.hpp"
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
        std::cout << "Listening on port 12345\n";
        kv::Socket client = server.accept();
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

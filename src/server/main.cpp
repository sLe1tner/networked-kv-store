
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
        std::cout << "Listening on port 12345\n";
        kv::Socket client = server.accept();

        kv::Connection client_connection{ std::move(client) };
        std::string_view data("test\n");
        client_connection.write(data);
        std::string read_data = client_connection.read_line();
        std::cout << "read: " << read_data << "\n";

        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

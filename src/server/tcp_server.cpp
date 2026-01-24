#include "tcp_server.hpp"

#include <stdexcept>     // std::runtime_error
#include <sys/socket.h>  // socket(), bind(), listen()
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons()
#include <unistd.h>      // close()

namespace kv {


void TcpServer::start(uint16_t port) {
    if (listening_)
        throw std::runtime_error("Server is already listening");

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        throw std::runtime_error("Failed to create socket");

    listen_socket_ = Socket(fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port); // Converts port to network byte order

    if (::bind(listen_socket_.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        throw std::runtime_error("Bind failed");

    if (::listen(listen_socket_.fd(), SOMAXCONN) == -1)
        throw std::runtime_error("Listen failed");

    listening_ = true;
}

void TcpServer::stop() {
    listening_ = false;
    listen_socket_ = Socket{}; // destroy old socket, closes FD
}

Socket TcpServer::accept() {
    if (!listening_)
        throw std::runtime_error("Server is not listening");

    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(
        listen_socket_.fd(),
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_len
    );

    if (client_fd == -1) {
        throw std::runtime_error("accept() failed");
    }

    return Socket{client_fd};
}

bool TcpServer::is_listening() const noexcept {
    return listening_;
}

} // namespace kv

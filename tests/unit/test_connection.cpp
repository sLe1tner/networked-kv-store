#include <gtest/gtest.h>
#include "connection.hpp"
#include "kv/socket.hpp"
#include <sys/socket.h>
#include <unistd.h>


using namespace kv;

class ConnectionTest : public ::testing::Test {
protected:
    int client_fd_;
    std::unique_ptr<Connection> connection;

    void SetUp() override {
        int fds[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
        client_fd_ = fds[1];
        connection = std::make_unique<kv::Connection>(Socket{fds[0]});
    }

    void TearDown() override {
        close(client_fd_);
    }

    void client_sends(const std::string& data) {
         [[maybe_unused]] ssize_t _ = write(client_fd_, data.data(), data.size());
    }

    std::string client_reads() {
        std::string data{};
        char buffer[4096];
        ssize_t n = 1;

        while ((n = read(client_fd_, buffer, 4096)) > 0) {
            data.append(buffer, n);
            if (n < 4096)
                break;
        }
        return data;
    }

};


TEST_F(ConnectionTest, BuffersPartialDataUntilNewline) {
    client_sends("SET key ");
    connection->read_to_inbox();
    EXPECT_TRUE(connection->inbox_has_data());
    EXPECT_EQ(connection->try_get_line(), std::nullopt);

    client_sends("value\n");
    connection->read_to_inbox();
    EXPECT_EQ(connection->try_get_line(), "SET key value");
}

TEST_F(ConnectionTest, MultipleCommandsInOneMessage) {
    client_sends("SET key value\nGET key\n");
    connection->read_to_inbox();
    EXPECT_TRUE(connection->inbox_has_data());
    EXPECT_EQ(connection->try_get_line(), "SET key value");
    EXPECT_EQ(connection->try_get_line(), "GET key");
}

TEST_F(ConnectionTest, MultipleCommandsInOneMessageCRLF) {
    client_sends("SET key value\r\nGET key\r\n");
    connection->read_to_inbox();
    EXPECT_TRUE(connection->inbox_has_data());
    EXPECT_EQ(connection->try_get_line(), "SET key value\r");
    EXPECT_EQ(connection->try_get_line(), "GET key\r");
}

TEST_F(ConnectionTest, ReadConnectionClosed) {
    close(client_fd_);
    EXPECT_FALSE(connection->read_to_inbox());
}

TEST_F(ConnectionTest, ReceiveEmptyLine) {
    client_sends("\n");
    connection->read_to_inbox();
    EXPECT_TRUE(connection->inbox_has_data());
    EXPECT_EQ(connection->try_get_line(), "");
    // rest is handled by protocol, not connection
}

TEST_F(ConnectionTest, ReadMessageLargerThanBuffer) {
    size_t payload_size = 16 * 1024;
    std::string large_data(payload_size, 'A'); // 16KB of 'A's
    std::string full_command = "SET key " + large_data + "\n";
    client_sends(full_command);

    // Read in a loop until we get the full line
    // This simulates the server's event loop calling read_to_inbox whenever the socket is ready.
    std::optional<std::string> result;
    int max_attempts = 20; // Prevent infinite loop if test fails
    while (!(result = connection->try_get_line()) && (max_attempts-- > 0)) {
        connection->read_to_inbox();
    }

    ASSERT_TRUE(result.has_value()) << "Failed to retrieve line after multiple reads";
    EXPECT_EQ(result.value().size(), full_command.size() - 1); // -1 for the \n
    EXPECT_EQ(result.value(), "SET key " + large_data);
}

TEST_F(ConnectionTest, WriteLargeMessage) {
    size_t payload_size = 16 * 1024;
    std::string large_data(payload_size, 'A'); // 16KB of 'A's
    std::string full_response = "$" + large_data + "\n";

    connection->append_response(full_response);
    EXPECT_TRUE(connection->outbox_has_data());
    EXPECT_FALSE(connection->write_from_outbox());
    EXPECT_EQ(client_reads(), full_response);
    EXPECT_FALSE(connection->outbox_has_data());
}

TEST_F(ConnectionTest, WriteResponseToClient) {
    connection->append_response("OK\n");
    EXPECT_TRUE(connection->outbox_has_data());
    EXPECT_FALSE(connection->write_from_outbox());
    EXPECT_EQ(client_reads(), "OK\n");
    EXPECT_FALSE(connection->outbox_has_data());
}

TEST_F(ConnectionTest, WriteConnectionClosed) {
    connection->append_response("OK\n");
    close(client_fd_);
    EXPECT_THROW(connection->write_from_outbox(), IOError);
}

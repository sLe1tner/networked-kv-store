#include <gtest/gtest.h>
#include "kv/protocol.hpp"

using namespace kv;

// Set

TEST(ProtocolTest, SetValidCommand) {
    Command result = Protocol::parse("SET key value");
    Set* set_cmd = std::get_if<Set>(&result);
    ASSERT_TRUE(set_cmd);
    EXPECT_EQ(set_cmd->key, "key");
    EXPECT_EQ(set_cmd->value, "value");
}

TEST(ProtocolTest, SetValidExtraSpace) {
    Command result = Protocol::parse("   SET      key      value");
    Set* set_cmd = std::get_if<Set>(&result);
    ASSERT_TRUE(set_cmd);
    EXPECT_EQ(set_cmd->key, "key");
    EXPECT_EQ(set_cmd->value, "value");
}

TEST(ProtocolTest, SetValidCaseInsensitive) {
    Command result = Protocol::parse("sEt kEy ValUE");
    Set* set_cmd = std::get_if<Set>(&result);
    ASSERT_TRUE(set_cmd);
    EXPECT_EQ(set_cmd->key, "kEy");
    EXPECT_EQ(set_cmd->value, "ValUE");
}

TEST(ProtocolTest, SetLargeValue) {
    std::string large_value(10000, 'A');
    std::string input = "SET key " + large_value;

    Command result = Protocol::parse(input);
    Set* set_cmd = std::get_if<Set>(&result);

    ASSERT_TRUE(set_cmd);
    EXPECT_EQ(set_cmd->value.size(), 10000);
}

TEST(ProtocolTest, SetMissingParameters) {
    EXPECT_THROW(Protocol::parse("SET"), ProtocolError);
    EXPECT_THROW(Protocol::parse("SET key"), ProtocolError);
}

TEST(ProtocolTest, SetTooManyParameters) {
    EXPECT_THROW(Protocol::parse("SET key value extra"), ProtocolError);
}


// Get

TEST(ProtocolTest, GetValidCommand) {
    Command result = Protocol::parse("GET key");
    Get* get_cmd = std::get_if<Get>(&result);
    ASSERT_TRUE(get_cmd);
    EXPECT_EQ(get_cmd->key, "key");
}

TEST(ProtocolTest, GetValidExtraSpace) {
    Command result = Protocol::parse("   GET   key");
    Get* get_cmd = std::get_if<Get>(&result);
    ASSERT_TRUE(get_cmd);
    EXPECT_EQ(get_cmd->key, "key");
}

TEST(ProtocolTest, GetValidCaseInsensitive) {
    Command result = Protocol::parse("geT key");
    Get* get_cmd = std::get_if<Get>(&result);
    ASSERT_TRUE(get_cmd);
    EXPECT_EQ(get_cmd->key, "key");
}

TEST(ProtocolTest, GetMissingParameters) {
    EXPECT_THROW(Protocol::parse("GET"), ProtocolError);
}

TEST(ProtocolTest, GetTooManyParameters) {
    EXPECT_THROW(Protocol::parse("GET key extra"), ProtocolError);
}


// Del

TEST(ProtocolTest, DelValidCommand) {
    Command result = Protocol::parse("DEL key");
    Del* del_cmd = std::get_if<Del>(&result);
    ASSERT_TRUE(del_cmd);
    EXPECT_EQ(del_cmd->key, "key");
}

TEST(ProtocolTest, DelValidExtraSpace) {
    Command result = Protocol::parse("   DEL    key");
    Del* del_cmd = std::get_if<Del>(&result);
    ASSERT_TRUE(del_cmd);
    EXPECT_EQ(del_cmd->key, "key");
}

TEST(ProtocolTest, DelValidCaseInsensitive) {
    Command result = Protocol::parse("del key");
    Del* del_cmd = std::get_if<Del>(&result);
    ASSERT_TRUE(del_cmd);
    EXPECT_EQ(del_cmd->key, "key");
}

TEST(ProtocolTest, DelMissingParameters) {
    EXPECT_THROW(Protocol::parse("DEL"), ProtocolError);
}

TEST(ProtocolTest, DelTooManyParameters) {
    EXPECT_THROW(Protocol::parse("DEL key extra"), ProtocolError);
}


// Other

TEST(ProtocolTest, EmptyInput) {
    Command result1 = Protocol::parse("");
    Command result2 = Protocol::parse("   ");

    NoOp* noop_cmd1 = std::get_if<NoOp>(&result1);
    NoOp* noop_cmd2 = std::get_if<NoOp>(&result2);

    ASSERT_TRUE(noop_cmd1);
    ASSERT_TRUE(noop_cmd2);
}

TEST(ProtocolTest, RejectUnknownCommand) {
    EXPECT_THROW(Protocol::parse("FLUSH"), ProtocolError);
}

TEST(ProtocolTest, FormatOk) {
    EXPECT_EQ(Protocol::format_ok(), "+OK\n");
}

TEST(ProtocolTest, FormatError) {
    EXPECT_EQ(Protocol::format_error("msg"), "-ERR msg\n");
}

TEST(ProtocolTest, FormatValue) {
    EXPECT_EQ(Protocol::format_value("msg"), "$msg\n");
}

TEST(ProtocolTest, KeysAreCaseSensitive) {
    Command result1 = Protocol::parse("GET key");
    Command result2 = Protocol::parse("GET KEY");

    Get* cmd1 = std::get_if<Get>(&result1);
    Get* cmd2 = std::get_if<Get>(&result2);

    EXPECT_NE(cmd1->key, cmd2->key); // "key" != "KEY"
}

TEST(ProtocolTest, CarriageReturn) {
    Command result = Protocol::parse("GET key\r");
    Get* get_cmd = std::get_if<Get>(&result);
    ASSERT_TRUE(get_cmd);
    EXPECT_EQ(get_cmd->key, "key");
}

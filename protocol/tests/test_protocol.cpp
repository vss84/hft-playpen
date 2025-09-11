#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "protocol/messages.h"
#include "protocol/binary_codec.h"
#include "protocol/message_dispatcher.h"

using namespace protocol;

static NewOrderMessage MakeNewOrderMsg(uint64_t order_id,
                                       uint32_t symbol_id,
                                       uint32_t price_ticks,
                                       uint32_t quantity,
                                       Side side = Side::BUY,
                                       OrderType type = OrderType::LIMIT,
                                       TimeInForce tif = TimeInForce::GTC,
                                       uint8_t version = 1)
{
    NewOrderMessage m{ };
    m.header.msg_length = sizeof(NewOrderMessage);
    m.header.msg_type = MessageType::NEW_ORDER;
    m.header.version = version;
    m.order_id = order_id;
    m.symbol_id = symbol_id;
    m.price_ticks = price_ticks;
    m.quantity = quantity;
    m.side = side;
    m.type = type;
    m.tif = tif;
    m.padding = 0;
    return m;
}

static CancelOrderMessage MakeCancelMsg(uint64_t order_id,
                                        uint32_t symbol_id,
                                        uint8_t version = 1)
{
    CancelOrderMessage m{ };
    m.header.msg_length = sizeof(CancelOrderMessage);
    m.header.msg_type = MessageType::CANCEL_ORDER;
    m.header.version = version;
    m.order_id = order_id;
    m.symbol_id = symbol_id;
    m.padding = 0;
    return m;
}

TEST(BinaryCodec_EncodeDecode, NewOrderRoundtrip) 
{
    NewOrderMessage src = MakeNewOrderMsg(12345, 7, 1000, 10, Side::SELL, OrderType::LIMIT, TimeInForce::GTC);
    auto buf = BinaryCodec::Encode(src);
    ASSERT_EQ(buf.size(), sizeof(NewOrderMessage));
    auto decoded = BinaryCodec::Decode<NewOrderMessage>(buf.data(), buf.size());
    EXPECT_EQ(decoded.order_id, src.order_id);
    EXPECT_EQ(decoded.symbol_id, src.symbol_id);
    EXPECT_EQ(decoded.price_ticks, src.price_ticks);
    EXPECT_EQ(decoded.quantity, src.quantity);
}

TEST(BinaryCodec_ParseHeader, HappyAndErrorPaths) 
{
    NewOrderMessage src = MakeNewOrderMsg(1, 1, 10, 1);
    auto buf = BinaryCodec::Encode(src);

    auto header = BinaryCodec::ParseHeader(buf.data(), buf.size());
    EXPECT_EQ(header.msg_type, MessageType::NEW_ORDER);
    EXPECT_EQ(header.msg_length, sizeof(NewOrderMessage));

    std::vector<uint8_t> tiny(buf.begin(), buf.begin() + (sizeof(MessageHeader) - 1));
    EXPECT_THROW(BinaryCodec::ParseHeader(tiny.data(), tiny.size()), std::runtime_error);

    MessageHeader fake_h{};
    fake_h.msg_length = 9999999;
    fake_h.msg_type = MessageType::NEW_ORDER;
    fake_h.version = 1;
    std::vector<uint8_t> fake_buf(sizeof(MessageHeader));
    std::memcpy(fake_buf.data(), &fake_h, sizeof(MessageHeader));
    EXPECT_THROW(BinaryCodec::ParseHeader(fake_buf.data(), fake_buf.size()), std::runtime_error);
}

TEST(BinaryCodec_TicksPriceConversion, Roundtrip) 
{
    double tick_size = 0.01;
    uint32_t ticks = 123;
    double price = BinaryCodec::TicksToPrice(ticks, tick_size);
    EXPECT_DOUBLE_EQ(price, 1.23);
    uint32_t ticks_back = BinaryCodec::PriceToTicks(price, tick_size);
    EXPECT_EQ(ticks_back, ticks);
}

TEST(MessageDispatcher_Deserialize, NewAndCancel) 
{
    NewOrderMessage newmsg = MakeNewOrderMsg(111, 2, 200, 5, Side::BUY, OrderType::LIMIT, TimeInForce::GTC);
    auto buf_new = BinaryCodec::Encode(newmsg);
    auto var_new = MessageDispatcher::Deserialize(buf_new.data(), buf_new.size());
    ASSERT_TRUE(std::holds_alternative<NewOrderMessage>(var_new));
    auto &nm = std::get<NewOrderMessage>(var_new);
    EXPECT_EQ(nm.order_id, newmsg.order_id);

    CancelOrderMessage cmsg = MakeCancelMsg(2222, 3);
    auto buf_cancel = BinaryCodec::Encode(cmsg);
    auto var_cancel = MessageDispatcher::Deserialize(buf_cancel.data(), buf_cancel.size());
    ASSERT_TRUE(std::holds_alternative<CancelOrderMessage>(var_cancel));
    auto &cm = std::get<CancelOrderMessage>(var_cancel);
    EXPECT_EQ(cm.order_id, cmsg.order_id);
}

TEST(MessageDispatcher_UnknownType, Throws) 
{
    MessageHeader h{};
    h.msg_length = sizeof(MessageHeader);
    h.msg_type = static_cast<MessageType>(0xFF);
    h.version = 1;
    std::vector<uint8_t> buf(sizeof(MessageHeader));
    std::memcpy(buf.data(), &h, sizeof(MessageHeader));
    EXPECT_THROW(MessageDispatcher::Deserialize(buf.data(), buf.size()), std::runtime_error);
}

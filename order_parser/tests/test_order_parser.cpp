#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "protocol/messages.h"
#include "protocol/binary_codec.h"
#include "order_parser/message_parser.h"

using namespace hft;

static protocol::NewOrderMessage MakeNewOrderMsg(uint64_t order_id,
                                                 uint32_t symbol_id,
                                                 uint32_t price_ticks,
                                                 uint32_t quantity,
                                                 protocol::Side side = protocol::Side::BUY,
                                                 protocol::OrderType type = protocol::OrderType::LIMIT,
                                                 protocol::TimeInForce tif = protocol::TimeInForce::GTC,
                                                 uint8_t version = 1)
{
    protocol::NewOrderMessage m{ };
    m.header.msg_length = sizeof(protocol::NewOrderMessage);
    m.header.msg_type = protocol::MessageType::NEW_ORDER;
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

static protocol::CancelOrderMessage MakeCancelMsg(uint64_t order_id,
                                                  uint32_t symbol_id,
                                                  uint8_t version = 1)
{
    protocol::CancelOrderMessage m{ };
    m.header.msg_length = sizeof(protocol::CancelOrderMessage);
    m.header.msg_type = protocol::MessageType::CANCEL_ORDER;
    m.header.version = version;
    m.order_id = order_id;
    m.symbol_id = symbol_id;
    m.padding = 0;
    return m;
}

TEST(MessageParser_NewOrder, ConvertsToOrderRequest) 
{
    uint64_t order_id = 555;
    uint32_t symbol_id = 42;
    uint32_t price_ticks = 250;
    uint32_t qty = 12;
    protocol::NewOrderMessage m = MakeNewOrderMsg(order_id, symbol_id, price_ticks, qty, 
                                                  protocol::Side::SELL,
                                                  protocol::OrderType::LIMIT,
                                                  protocol::TimeInForce::IOC);
    auto buf = protocol::BinaryCodec::Encode(m);

    MessageParser parser;
    auto req = parser.ParseMessage(buf);

    EXPECT_EQ(req.type, RequestType::NEW_ORDER);
    EXPECT_EQ(req.order.id, order_id);
    EXPECT_EQ(req.order.symbol_id, symbol_id);
    EXPECT_DOUBLE_EQ(req.order.price, double(price_ticks) * 0.01);
    EXPECT_EQ(req.order.quantity, qty);
    EXPECT_EQ(req.order.side, Side::SELL);
    EXPECT_EQ(req.order.tif, TimeInForce::IOC);
}

TEST(MessageParser_CancelOrder, ConvertsToCancelRequest) 
{
    uint64_t order_id = 9999;
    uint32_t symbol_id = 55;
    protocol::CancelOrderMessage c = MakeCancelMsg(order_id, symbol_id);
    auto buf = protocol::BinaryCodec::Encode(c);

    MessageParser parser;
    auto req = parser.ParseMessage(buf);

    EXPECT_EQ(req.type, RequestType::CANCEL_ORDER);
    EXPECT_EQ(req.order_id_to_cancel, order_id);
    EXPECT_EQ(req.symbol_id, symbol_id);
}

TEST(MessageParser_InvalidBuffer, ThrowsOnBadInput) 
{
    MessageParser parser;
    std::vector<uint8_t> tiny(2, 0);
    EXPECT_THROW(parser.ParseMessage(tiny), std::runtime_error);
}

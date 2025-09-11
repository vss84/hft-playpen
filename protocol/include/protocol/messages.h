#ifndef MESSAGES_H
#define MESSAGES_H

#include <cstdint>

namespace protocol
{
    enum class MessageType : uint8_t
    {
        NEW_ORDER = 0,
        CANCEL_ORDER = 1,
        MODIFY_ORDER = 2,
    };

    enum class Side : uint8_t
    {
        BUY = 0,
        SELL = 1
    };

    enum class OrderType : uint8_t
    {
        LIMIT = 0,
        MARKET = 1
    };

    enum class TimeInForce : uint8_t
    {
        GTC = 0,
        IOC = 1,
        FOK = 2,
    };

#pragma pack(push, 1)
    struct MessageHeader
    {
        uint64_t msg_length;
        MessageType msg_type;
        uint8_t version;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct NewOrderMessage
    {
        MessageHeader header;
        uint64_t order_id;
        uint32_t symbol_id;
        uint32_t price_ticks;
        uint32_t quantity;
        Side side;
        OrderType type;
        TimeInForce tif;
        uint8_t padding;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct CancelOrderMessage
    {
        MessageHeader header;
        uint64_t order_id;
        uint32_t symbol_id;
        uint32_t padding;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct ModifyOrderMessage
    {
        MessageHeader header;
        uint64_t order_id;
        uint32_t symbol_id;
        uint32_t new_price_ticks;
        uint32_t new_quantity;
        uint32_t padding;
    };
#pragma pack(pop)

} // namespace protocol

#endif // MESSAGES_H
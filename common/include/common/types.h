#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

using OrderId = uint64_t;
using Price = double;
using Quantity = uint32_t;

enum class Side
{
    BUY,
    SELL
};

enum class OrderType
{
    LIMIT,
    MARKET
};

enum class OrderStatus
{
    NEW,
    ACTIVE,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED,
    REJECTED
};

enum class TimeInForce
{
    GTC,
    IOC,
    FOK,
};

enum class RequestType
{
    NEW_ORDER,
    CANCEL_ORDER,
    MODIFY_ORDER,
};

struct TradeEvent
{
    OrderId maker_order_id;
    OrderId taker_order_id;
    Price price;
    Quantity quantity;
    uint64_t timestamp_ns;
};

class Order
{
public:
    OrderId id;
    uint32_t symbol_id;
    Side side;
    Price price;
    Quantity quantity;
    Quantity filled_qty{ 0 };
    OrderType type;
    TimeInForce tif;
    OrderStatus status;
    uint64_t timestamp_ns;
    uint64_t sequence_id;

    bool IsActive() const { return status == OrderStatus::ACTIVE; }
    bool IsComplete() const { return status == OrderStatus::FILLED || status == OrderStatus::CANCELLED; }
    Quantity RemainingQuantity() const { return (filled_qty >= quantity) ? 0u : (quantity - filled_qty); }
};

struct OrderRequest
{
    RequestType type;
    Order order;
    OrderId order_id_to_cancel;
    uint32_t symbol_id;
    uint64_t timestamp_ns;
};

#endif // TYPES_H
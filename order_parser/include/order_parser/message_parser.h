#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <variant>
#include <chrono>

#include "protocol/message_dispatcher.h"
#include "common/types.h"

namespace hft
{
    class MessageParser
    {
    public:
        OrderRequest ParseMessage(const std::vector<uint8_t> &buffer)
        {
            auto message = protocol::MessageDispatcher::Deserialize(buffer.data(), buffer.size());

            OrderRequest request;

            std::visit(
                [this, &request](const auto &msg)
                {
                    using T = std::decay_t<decltype(msg)>;
                    if constexpr (std::is_same_v<T, protocol::NewOrderMessage>)
                    {
                        request = HandleNewOrder(msg);
                    }
                    else if constexpr (std::is_same_v<T, protocol::CancelOrderMessage>)
                    {
                        request = HandleCancel(msg);
                    }
                    else if constexpr (std::is_same_v<T, protocol::ModifyOrderMessage>)
                    {
                        throw std::runtime_error("Modify not implemented");
                    }
                }, message);

            return request;
        }
    
    private:
        static Side ConvertSide(protocol::Side side)
        {
            switch (side)
            {
                case protocol::Side::BUY: return Side::BUY;
                case protocol::Side::SELL: return Side::SELL;
                default: throw std::invalid_argument("Invalid protocol side");
            }
        }

        static TimeInForce ConvertTif(protocol::TimeInForce tif)
        {
            switch (tif)
            {
                case protocol::TimeInForce::FOK: return TimeInForce::FOK;
                case protocol::TimeInForce::GTC: return TimeInForce::GTC;
                case protocol::TimeInForce::IOC: return TimeInForce::IOC;
                default: throw std::invalid_argument("Invalid protocol TIF");
            }
        }

        static OrderType ConvertType(protocol::OrderType type)
        {
            switch (type)
            {
                case protocol::OrderType::LIMIT: return OrderType::LIMIT;
                case protocol::OrderType::MARKET: return OrderType::MARKET;
                default: throw std::invalid_argument("Invalid protocol OrderType");
            }
        }

        Order MessageToOrder(const protocol::NewOrderMessage &msg, double tick_size = 0.01)
        {
            Order order;
            order.id = msg.order_id;
            order.symbol_id = msg.symbol_id;
            order.price = msg.price_ticks * tick_size;
            order.quantity = msg.quantity;
            order.side = ConvertSide(msg.side);
            order.tif = ConvertTif(msg.tif);
            order.type = ConvertType(msg.type);
            return order;
        }

        OrderRequest HandleNewOrder(const protocol::NewOrderMessage &msg)
        {
            auto order = MessageToOrder(msg);
            OrderRequest request;
            request.type = RequestType::NEW_ORDER;
            request.order = std::move(order);
            request.symbol_id = msg.symbol_id;
            request.timestamp_ns = 0; // TODO
            return request;
        }

        OrderRequest HandleCancel(const protocol::CancelOrderMessage &msg)
        {
            OrderRequest request;
            request.type = RequestType::CANCEL_ORDER;
            request.order_id_to_cancel = msg.order_id;
            request.symbol_id = msg.symbol_id;
            request.timestamp_ns = 0;
            return request;
        }

        uint64_t GetTimestamp()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        }

    };

}// namespace hft

#endif // MESSAGE_PARSER_H
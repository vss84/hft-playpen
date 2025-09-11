#ifndef MATCHING_ENGINE_H
#define MATCHING_ENGINE_H

#include <memory>
#include <vector>
#include <chrono>
#include <optional>

#include "common/types.h"
#include "orderbook/orderbook.h"

namespace hft
{
    class MatchingEngine
    {
    public:
        MatchingEngine() = default;

        void ProcessOrderRequest(const OrderRequest &request)
        {
            switch (request.type)
            {
                case RequestType::NEW_ORDER:    
                    ProcessNewOrder(request.order);
                    break;

                case RequestType::CANCEL_ORDER: 
                    ProcessCancelOrder(request.order_id_to_cancel);
                    break;

                case RequestType::MODIFY_ORDER: 
                    // TODO
                    break;
            }
        }

        std::vector<TradeEvent> GetAndClearTrades()
        {
            std::vector<TradeEvent> trades;
            trades.swap(m_trades);
            return trades;
        }

    private:
        Orderbook m_orderbook;
        uint64_t m_next_order_id{ 1 };
        uint64_t m_global_seq{ 0 };
        std::vector<TradeEvent> m_trades;

        static uint64_t GetTimestampInNs()
        {
            using namespace std::chrono;
            return duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()).count();
        }

        void ProcessNewOrder(Order order)
        {
            if (order.id == 0) order.id = m_next_order_id++;
            order.sequence_id = ++m_global_seq;
            order.timestamp_ns = GetTimestampInNs();
            order.status = OrderStatus::ACTIVE;

            bool is_market = (order.type == OrderType::MARKET);
            bool fully_filled = TryMatch(order, is_market);

            if (!fully_filled)
            {
                auto remainder = order.RemainingQuantity();
                if (remainder > 0)
                {
                    if (order.type == OrderType::LIMIT && order.tif == TimeInForce::GTC)
                    {
                        m_orderbook.AddOrder(std::make_unique<Order>(order));
                    }
                    else if (order.tif == TimeInForce::FOK)
                    {
                        order.status = OrderStatus::REJECTED;
                    }
                    else
                    {
                        order.status = (order.filled_qty > 0) ? OrderStatus::PARTIALLY_FILLED : OrderStatus::CANCELLED;
                    }
                }
                else
                {
                    order.status = OrderStatus::FILLED;
                }
            }
            else
            {
                order.status = OrderStatus::FILLED;
            }

            // NOTE(vss): we could log order acceptance + fills here
        }

        bool TryMatch(Order &incoming_order, bool is_market)
        {
            Side opposite_side = (incoming_order.side == Side::BUY) ? Side::SELL : Side::BUY;

            if (incoming_order.tif == TimeInForce::FOK)
            {
                Quantity available = AvailableQuantityFor(incoming_order, is_market);
                if (available < incoming_order.RemainingQuantity())
                {
                    incoming_order.status = OrderStatus::REJECTED;
                    return false;
                }
            }

            bool opposite_has = (opposite_side == Side::BUY) ? m_orderbook.HasBids() : m_orderbook.HasAsks();
            if (!opposite_has && is_market) { return false; }

            while (incoming_order.RemainingQuantity() > 0)
            {
                Order *maker = m_orderbook.GetBestOrder(opposite_side);
                if (!maker) { break; }

                std::optional<Price> best_price;
                if (opposite_side == Side::BUY) best_price = m_orderbook.BestBid();
                else best_price = m_orderbook.BestAsk();

                if (!best_price.has_value()) { break; }
                Price execution_price = best_price.value();

                if (!is_market)
                {
                    if (incoming_order.side == Side::BUY &&
                        incoming_order.price < execution_price)
                    {
                        break;
                    }

                    if (incoming_order.side == Side::SELL &&
                        incoming_order.price > execution_price) 
                    {
                        break;
                    }
                }

                Quantity trade_qty = std::min(incoming_order.RemainingQuantity(), maker->RemainingQuantity());
                if (trade_qty == 0) { break; }

                incoming_order.filled_qty += trade_qty;
                maker->filled_qty += trade_qty;

                TradeEvent event;
                event.maker_order_id = maker->id;
                event.taker_order_id = incoming_order.id;
                event.price = execution_price;
                event.quantity = trade_qty;
                event.timestamp_ns = GetTimestampInNs();
                m_trades.push_back(event);

                if (maker->RemainingQuantity() == 0)
                {
                    m_orderbook.RemoveOrder(maker->id);
                }
            }

            if (incoming_order.RemainingQuantity() == 0)
            {
                incoming_order.status = OrderStatus::FILLED;
                return true;
            }

            if (incoming_order.filled_qty > 0)
            {
                incoming_order.status = OrderStatus::PARTIALLY_FILLED;
            }
            return false;
        }

        Quantity AvailableQuantityFor(const Order &incoming_order, bool is_market)
        {
            Quantity sum = 0;
            
            // TODO(vss): this is not thread-safe across modifications!!
            if (incoming_order.side == Side::BUY)
            {
                auto best_ask = m_orderbook.BestAsk();
                while (best_ask)
                {
                    Price price = *best_ask;
                    if (!is_market && incoming_order.price < price) { break; }

                    Order *maker = m_orderbook.GetBestOrder(Side::SELL);
                    if (!maker) { break; }

                    sum += maker->RemainingQuantity();
                    if (sum >= incoming_order.RemainingQuantity()) { return sum; }
                    break;
                }
            }
            else
            {
                auto best_bid = m_orderbook.BestBid();
                while (best_bid)
                {
                    Price price = *best_bid;
                    if (!is_market && incoming_order.price > price) { break; }

                    Order *maker = m_orderbook.GetBestOrder(Side::BUY);
                    if (!maker) { break; }

                    sum += maker->RemainingQuantity();
                    if (sum >= incoming_order.RemainingQuantity()) { return sum; }
                    break;
                }
            }

            return sum;
        }

        void ProcessCancelOrder(OrderId order_id)
        {
            Order *order = m_orderbook.GetOrder(order_id);
            if (!order) { return; }
            order->status = OrderStatus::CANCELLED;
            m_orderbook.RemoveOrder(order_id);
        }
    };

} // namespace hft

#endif // MATCHING_ENGINE_H
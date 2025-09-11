#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <map>
#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <optional>

#include "common/types.h"

namespace hft
{
    class Orderbook
    {
    public:
        Orderbook() = default;

        void AddOrder(std::unique_ptr<Order> order)
        {
            if (!order) { return; }

            Price price = order->price;
            if (order->side == Side::BUY)
            {
                auto &level = m_bids[price];
                level.level_orders.push_back(std::move(order));
                auto it = std::prev(level.level_orders.end());
                level.level_qty += (*it)->RemainingQuantity();
                m_order_info[(*it)->id] = { price, it };
            }
            else
            {
                auto &level = m_asks[price];
                level.level_orders.push_back(std::move(order));
                auto it = std::prev(level.level_orders.end());
                level.level_qty += (*it)->RemainingQuantity();
                m_order_info[(*it)->id] = { price, it };
            }
            
        }

        void RemoveOrder(OrderId order_id)
        {
            auto order_it = m_order_info.find(order_id);
            if (order_it == m_order_info.end()) { return; }
            Price price = order_it->second.price;
            auto &it = order_it->second.it;

            Order *stored = it->get();
            if (!stored)
            {
                m_order_info.erase(order_it);
                return;
            }

            if (stored->side == Side::BUY) 
            {
                auto level_it = m_bids.find(price);
                if (level_it != m_bids.end()) 
                {
                    level_it->second.level_qty -= (*it)->RemainingQuantity();
                    level_it->second.level_orders.erase(it);
                    if (level_it->second.level_orders.empty())
                    {
                        m_bids.erase(level_it);
                    }
                }
            }
            else 
            {
                auto level_it = m_asks.find(price);
                if (level_it != m_asks.end()) 
                {
                    level_it->second.level_qty -= (*it)->RemainingQuantity();
                    level_it->second.level_orders.erase(it);
                    if (level_it->second.level_orders.empty()) 
                    {
                        m_asks.erase(level_it);
                    }
                }
            }

            m_order_info.erase(order_it);
        }

        void ModifyOrder(OrderId order_id)
        {
            // TODO
            return;
        }

        Order *GetOrder(OrderId order_id)
        {
            auto order_it = m_order_info.find(order_id);
            if (order_it == m_order_info.end()) { return nullptr; }
            return order_it->second.it->get();
        }

        bool HasBids() const { return !m_bids.empty(); }
        bool HasAsks() const { return !m_asks.empty(); }

        std::optional<Price> BestBid() const
        {
            if (m_bids.empty()) { return std::nullopt; }
            return m_bids.begin()->first;
        }

        std::optional<Price> BestAsk() const
        {
            if (m_asks.empty()) { return std::nullopt; }
            return m_asks.begin()->first;
        }

        Order *GetBestOrder(Side side)
        {
            if (side == Side::BUY)
            {
                if (m_bids.empty()) { return nullptr; }
                auto &level = m_bids.begin()->second;
                if (level.level_orders.empty()) { return nullptr; }
                return level.level_orders.front().get();
            }
            else
            {
                if (m_asks.empty()) { return nullptr; }
                auto &level = m_asks.begin()->second;
                if (level.level_orders.empty()) { return nullptr; }
                return level.level_orders.front().get();
            }
        }

        struct LevelInfo 
        {   Price price;
            Quantity quantity;
            size_t orders; 
        };

        struct Snapshot 
        { 
            std::vector<LevelInfo> bids; 
            std::vector<LevelInfo> asks; 
            uint64_t seq; 
        };

        Snapshot SnapshotTop(size_t depth = 5) const
        {
            Snapshot snap;
            snap.seq = m_seq_num;
            size_t count = 0;
            for (auto it = m_bids.begin(); it != m_bids.end() && count < depth; ++it, ++count)
            {
                snap.bids.push_back({ it->first, it->second.level_qty, it->second.level_orders.size() });
            }

            count = 0;
            for (auto it = m_asks.begin(); it != m_asks.end() && count < depth; ++it, ++count)
            {
                snap.asks.push_back({ it->first, it->second.level_qty, it->second.level_orders.size() });
            }

            return snap;
        }
    private:
        struct LevelData
        {
            std::list<std::unique_ptr<Order>> level_orders;
            Quantity level_qty;
        };

        struct OrderIndex
        {
            Price price;
            std::list<std::unique_ptr<Order>>::iterator it;
        };

        std::unordered_map<OrderId, OrderIndex> m_order_info;
        std::map<Price, LevelData, std::greater<Price>> m_bids;
        std::map<Price, LevelData, std::less<Price>> m_asks;
        uint64_t m_seq_num = 0;
    };

} //namespace hft

#endif // ORDERBOOK_H
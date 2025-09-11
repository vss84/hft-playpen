#ifndef ORDER_GENERATOR_H
#define ORDER_GENERATOR_H

#include <random>
#include <chrono>
#include <unordered_set>
#include "common/types.h"

namespace hft
{
    class OrderGenerator
    {
    private:
        std::mt19937 m_rng;

        std::normal_distribution<> m_price_dist;
        std::poisson_distribution<> m_quantity_dist;
        std::bernoulli_distribution m_side_dist;
        std::discrete_distribution<> m_action_dist;
        std::discrete_distribution<> m_tif_dist;
        std::exponential_distribution<> m_arrival_dist;

        std::unordered_set<OrderId> m_active_orders;
        std::vector<OrderId> m_order_vector;

        double m_mid_price;
        double m_tick_size;
        uint32_t m_symbol_id;
        OrderId m_next_order_id{ 1000 };

        size_t m_total_orders{ 0 };
        size_t m_total_cancels{ 0 };

    public:
        OrderGenerator(uint32_t symbol_id = 1, double initial_mid_price = 100.0,
                       double tick_size = 0.01, unsigned seed = std::random_device{ }())
            : m_rng(seed)
            , m_price_dist(initial_mid_price, 0.5)  // spread
            , m_quantity_dist(100)  // avg qty
            , m_side_dist(0.5)  // 50/50 buy/sell
            , m_action_dist({ 70, 25, 5 })  // 70% new, 25% cancel, 5% modify
            , m_tif_dist({ 80, 15, 5 })  // 80% gtc, 15% ioc, 5% fok
            , m_arrival_dist(100.0)  // avg orders/sec
            , m_mid_price(initial_mid_price)
            , m_tick_size(tick_size)
            , m_symbol_id(symbol_id)
        {
        }

        OrderRequest GenerateNext()
        {
            if (m_total_orders % 100 == 0)
            {
                std::normal_distribution<> drift_dist(0.0, 0.1);
                m_mid_price += drift_dist(m_rng);
                m_price_dist = std::normal_distribution<>(m_mid_price, 0.5);
            }

            int action = m_action_dist(m_rng);

            switch (action)
            {
                case 0:
                    return GenerateNewOrder();
                case 1:
                    return GenerateCancelOrder();
                case 2:
                    return GenerateModifyOrder();
                default:
                    return GenerateNewOrder();
            }
        }

        std::vector<OrderRequest> GenerateBurst(size_t count)
        {
            std::vector<OrderRequest> requests;
            requests.reserve(count);

            for (size_t i = 0; i < count; ++i)
            {
                requests.push_back(GenerateNext());
            }

            return requests;
        }

        uint64_t GetNextArrivalTime()
        {
            return static_cast<uint64_t>(m_arrival_dist(m_rng) * 1000);
        }

    private:
        OrderRequest GenerateNewOrder()
        {
            Order order;
            order.id = m_next_order_id++;
            order.symbol_id = m_symbol_id;

            double raw_price = m_price_dist(m_rng);

            order.price = std::round(raw_price / m_tick_size) * m_tick_size;

            order.price = std::max(m_tick_size, order.price);

            order.quantity = std::max(1u, static_cast<uint32_t>(m_quantity_dist(m_rng)));

            order.side = m_side_dist(m_rng) ? Side::BUY : Side::SELL;

            if (order.side == Side::BUY)
            {
                order.price -= m_tick_size * (1 + (m_rng() % 5));
            }
            else
            {
                order.price += m_tick_size * (1 + (m_rng() % 5));
            }

            // Time in force
            int tif_choice = m_tif_dist(m_rng);
            order.tif = (tif_choice == 0) ? TimeInForce::GTC : 
                        (tif_choice == 1) ? TimeInForce::IOC : TimeInForce::FOK;

            order.timestamp_ns = GetTimestamp();
            order.status = OrderStatus::ACTIVE;

            m_active_orders.insert(order.id);
            m_order_vector.push_back(order.id);
            m_total_orders++;

            OrderRequest request;
            request.type = RequestType::NEW_ORDER;
            request.order = order;
            request.symbol_id = m_symbol_id;
            request.timestamp_ns = order.timestamp_ns;

            return request;
        }

        OrderRequest GenerateCancelOrder()
        {
            OrderRequest request;
            request.type = RequestType::CANCEL_ORDER;
            request.symbol_id = m_symbol_id;
            request.timestamp_ns = GetTimestamp();

            if (!m_active_orders.empty())
            {
                size_t idx = m_rng() % m_order_vector.size();
                request.order_id_to_cancel = m_order_vector[idx];

                m_active_orders.erase(request.order_id_to_cancel);
                m_order_vector.erase(m_order_vector.begin() + idx);
                m_total_cancels++;
            }
            else
            {
                return GenerateNewOrder();
            }

            return request;
        }

        OrderRequest GenerateModifyOrder()
        {
            if (!m_active_orders.empty())
            {
                return GenerateCancelOrder();
            }
            return GenerateNewOrder();
        }

        static uint64_t GetTimestamp()
        {
            using namespace std::chrono;
            return duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count();
        }
    };

} // namespace hft

#endif // ORDER_GENERATOR_H
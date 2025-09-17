#ifndef TRADING_PIPELINE_H
#define TRADING_PIPELINE_H

#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>
#include <chrono>
#include <format>

#include "ring_buffer/ring_buffer.h"
#include "order_generator/order_generator.h"
#include "order_parser/message_parser.h"
#include "matching_engine/matching_engine.h"
#include "common/types.h"
#include "logger/logger.h"

namespace hft
{
    class TradingPipeline
    {
    private:
        static constexpr size_t RING_BUFFER_SIZE = 1024;

        SPSCRingBuffer<std::vector<uint8_t>, RING_BUFFER_SIZE> m_agent_to_parser;
        SPSCRingBuffer<OrderRequest, RING_BUFFER_SIZE> m_parser_to_engine;
        SPSCRingBuffer<TradeEvent, RING_BUFFER_SIZE> m_engine_to_logger;

        OrderGenerator m_generator;
        MessageParser m_parser;
        MatchingEngine m_engine;
        Logger m_logger;

        std::thread m_agent_thread;
        std::thread m_parser_thread;
        std::thread m_engine_thread;
        std::thread m_logger_thread;

        std::atomic<bool> m_running{ false };
        std::atomic<uint64_t> m_orders_generated{ 0 };
        std::atomic<uint64_t> m_orders_parsed{ 0 };
        std::atomic<uint64_t> m_orders_matched{ 0 };
        std::atomic<uint64_t> m_trades_logged{ 0 };


    public:
        TradingPipeline(uint32_t symbol_id = 1)
            : m_generator(symbol_id)
            , m_logger("trades.log")
        { 
            m_logger.Log(LogLevel::INFO, "timestamp_ns, maker_id, taker_id, price, quantity");
        }

        ~TradingPipeline()
        {
            Stop();
        }

        void Start()
        {
            if (m_running.exchange(true))
                return;

            std::cout << "Starting trading pipeline...\n";

            m_logger_thread = std::thread(&TradingPipeline::LoggerThread, this);
            m_engine_thread = std::thread(&TradingPipeline::EngineThread, this);
            m_parser_thread = std::thread(&TradingPipeline::ParserThread, this);
            m_agent_thread = std::thread(&TradingPipeline::AgentThread, this);

            std::cout << "Pipeline started with 4 threads\n";
        }

        void Stop()
        {
            if (!m_running.exchange(false))
                return;

            std::cout << "Stopping trading pipeline...\n";

            if (m_agent_thread.joinable())
                m_agent_thread.join();
            if (m_parser_thread.joinable())
                m_parser_thread.join();
            if (m_engine_thread.joinable())
                m_engine_thread.join();
            if (m_logger_thread.joinable())
                m_logger_thread.join();

            m_logger.Flush();

            PrintStats();
        }

        void PrintStats() const
        {
            std::cout << "\n=== Pipeline Statistics ===\n";
            std::cout << "Orders Generated: " << m_orders_generated.load() << "\n";
            std::cout << "Orders Parsed: " << m_orders_parsed.load() << "\n";
            std::cout << "Orders Matched: " << m_orders_matched.load() << "\n";
            std::cout << "Trades Logged: " << m_trades_logged.load() << "\n";
            std::cout << "========================\n";
        }

    private:
        void AgentThread()
        {
            std::cout << "Agent thread started\n";

            while (m_running.load())
            {
                auto request = m_generator.GenerateNext();

                std::vector<uint8_t> buffer;

                if (request.type == RequestType::NEW_ORDER)
                {
                    protocol::NewOrderMessage msg{};
                    msg.header.msg_type = protocol::MessageType::NEW_ORDER;
                    msg.header.msg_length = sizeof(msg);
                    msg.header.version = 1;
                    msg.order_id = request.order.id;
                    msg.symbol_id = request.order.symbol_id;
                    msg.price_ticks = static_cast<uint32_t>(request.order.price / 0.01);
                    msg.quantity = request.order.quantity;
                    msg.side = (request.order.side == Side::BUY) ?
                        protocol::Side::BUY : protocol::Side::SELL;
                    msg.tif = ConvertTif(request.order.tif);

                    buffer = protocol::BinaryCodec::Encode(msg);
                }
                else if (request.type == RequestType::CANCEL_ORDER)
                {
                    protocol::CancelOrderMessage msg{};
                    msg.header.msg_type = protocol::MessageType::CANCEL_ORDER;
                    msg.header.msg_length = sizeof(msg);
                    msg.header.version = 1;
                    msg.order_id = request.order_id_to_cancel;
                    msg.symbol_id = request.symbol_id;

                    buffer = protocol::BinaryCodec::Encode(msg);
                }

                while (!m_agent_to_parser.TryPush(buffer))
                {
                    if (!m_running.load())
                        return;
                    std::this_thread::yield();
                }

                m_orders_generated.fetch_add(1);

                auto sleep_us = m_generator.GetNextArrivalTime();
                std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
            }

            std::cout << "Agent thread stopped\n";
        }

        void ParserThread()
        {
            std::cout << "Parser thread started\n";

            std::vector<uint8_t> buffer;

            while (m_running.load())
            {
                if (m_agent_to_parser.TryPop())
                {
                    OrderRequest request = m_parser.ParseMessage(buffer);

                    while (!m_parser_to_engine.TryPush(request))
                    {
                        if (!m_running.load())
                            return;
                        std::this_thread::yield();
                    }

                    m_orders_parsed.fetch_add(1);
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }

            std::cout << "Parser thread stopped\n";
        }

        void EngineThread()
        {
            std::cout << "Engine thread started\n";

            OrderRequest request;

            while (m_running.load())
            {
                if (m_parser_to_engine.TryPop())
                {
                    m_engine.ProcessOrderRequest(request);

                    auto trades = m_engine.GetAndClearTrades();

                    for (const auto &trade : trades)
                    {
                        while (!m_engine_to_logger.TryPush(trade))
                        {
                            if (!m_running.load())
                                return;
                            std::this_thread::yield();
                        }
                    }

                    m_orders_matched.fetch_add(1);
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }

            std::cout << "Engine thread stopped\n";
        }

        void LoggerThread()
        {
            std::cout << "Logger thread started\n";

            TradeEvent trade;
            size_t batch_count = 0;

            while (m_running.load())
            {
                if (m_engine_to_logger.TryPop())
                {
                    std::string trade_msg = std::format("{},{},{},{},{}",
                                                        trade.timestamp_ns,
                                                        trade.maker_order_id,
                                                        trade.taker_order_id,
                                                        trade.price,
                                                        trade.quantity);
                    
                    m_logger.Log(LogLevel::INFO, trade_msg);

                    m_trades_logged.fetch_add(1);
                    batch_count++;

                    if (batch_count >= 100)
                    {
                        m_logger.Flush();
                        batch_count = 0;
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }

            m_logger.Flush();
            std::cout << "Logger thread stopped\n";
        }

        static protocol::TimeInForce ConvertTif(TimeInForce tif)
        {
            switch (tif)
            {
                case TimeInForce::GTC: return protocol::TimeInForce::GTC;
                case TimeInForce::IOC: return protocol::TimeInForce::IOC;
                case TimeInForce::FOK: return protocol::TimeInForce::FOK;
                default: return protocol::TimeInForce::GTC;
            }
        }
    };

} // namespace hft

#endif // TRADING_PIPELINE_H
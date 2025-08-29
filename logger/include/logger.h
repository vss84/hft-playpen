#if !defined(LOGGER_H)

#include <string>
#include <cstring>
#include <fstream>
#include <thread>
#include <algorithm>
#include <vector>
#include <stdexcept>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_WIN_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#undef ERROR

#include "../../ring_buffer/include/ring_buffer.h"

namespace hft
{
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR };
    enum class OverflowPolicy { Drop, Block };

    class Logger
    {
        struct alignas(64) LogEntry
        {
            uint64_t timestamp_ns;
            LogLevel level;
            uint32_t thread_id;
            uint16_t payload_len;
            char payload[256];
        };

    public:
        Logger(const std::string &filename, OverflowPolicy policy = OverflowPolicy::Drop)
            : m_out(filename, std::ios::binary | std::ios::app)
            , m_policy(policy)
        {
            if (!m_out.is_open())
            {
                throw std::runtime_error("Failed to open log file: " + filename);
            }

            LARGE_INTEGER qpc_freq;
            if (!QueryPerformanceFrequency(&qpc_freq))
            {
                throw std::runtime_error("QueryPerformanceFrequency failed");
            }
            m_qpc_to_ns = 1e9 / static_cast<double>(qpc_freq.QuadPart);

            m_log_flusher = std::thread(&Logger::FlusherThreadFn, this);
        }

        ~Logger()
        {
            m_running.store(false, std::memory_order_release);
            if (m_log_flusher.joinable()) m_log_flusher.join();
            Flush();
            if (m_out.is_open()) m_out.flush();
        }

        bool Log(LogLevel level, const std::string &message)
        {
            LogEntry entry;
            entry.timestamp_ns = Now();
            entry.level = level;
            entry.thread_id = static_cast<uint32_t>(GetCurrentThreadId());
            
            entry.payload_len = std::min(message.size(), sizeof(entry.payload) - 1);
            memcpy(entry.payload, message.data(), entry.payload_len);
            entry.payload[entry.payload_len] = '\0';
            // NOTE(vss): We check for the min size, and overwrite the last char
            // with the null-terminator, provides same effect without needing
            // any extra checking or enforcing bounds/sizes.

            if (m_policy == OverflowPolicy::Drop)
            {
                if (!m_buffer.Push(entry))
                {
                    m_dropped.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                
                m_enqueued.fetch_add(1, std::memory_order_relaxed);
                return true;
                
            }
            else
            {
                // TODO(vss): Switch temporary implementation to a complete blocking policy.
                for (;;)
                {
                    if (m_buffer.Push(entry))
                    {
                        m_enqueued.fetch_add(1);
                        return true;
                    }
                    std::this_thread::yield();
                }
            }
        }

        void Flush()
        {
            while (!m_buffer.Empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            m_out.flush();
        }

        uint64_t dropped() const noexcept { return m_dropped.load(std::memory_order_relaxed); }
        uint64_t enqueued() const noexcept { return m_enqueued.load(std::memory_order_relaxed); }
    
    private:
        using RingBuffer = hft::SPSCRingBuffer<LogEntry, 1024>;
        
        RingBuffer m_buffer;
        std::ofstream m_out;
        OverflowPolicy m_policy;
        std::thread m_log_flusher;
        std::atomic<bool> m_running{ true };
        std::atomic<uint64_t> m_dropped{ 0 };
        std::atomic<uint64_t> m_enqueued{ 0 };
        double m_qpc_to_ns{ 0.0 };

        inline uint64_t Now() noexcept
        {
            LARGE_INTEGER qpc_now;
            QueryPerformanceCounter(&qpc_now);
            return static_cast<uint64_t>(qpc_now.QuadPart * m_qpc_to_ns);
        }

        void FlusherThreadFn()
        {
            constexpr size_t BATCH_SIZE = 256;
            constexpr auto IDLE_SLEEP = std::chrono::microseconds(50);
            std::vector<LogEntry> batch;
            batch.reserve(BATCH_SIZE);

            while (m_running.load(std::memory_order_acquire) || !m_buffer.Empty())
            {
                batch.clear();
                LogEntry tmp;
                
                for (size_t i = 0; i < BATCH_SIZE && m_buffer.Pop(tmp); ++i)
                {
                    batch.push_back(tmp);
                }

                if (batch.empty())
                {
                    std::this_thread::sleep_for(IDLE_SLEEP);
                    continue;
                }

                for (auto &log : batch)
                {
                    const char *lvl = (log.level == LogLevel::DEBUG) ? "DEBUG" :
                                      (log.level == LogLevel::INFO) ? "INFO" :
                                      (log.level == LogLevel::WARNING) ? "WARNING" : "ERROR";

                    m_out << log.timestamp_ns << ' ' 
                          << log.thread_id << ' ' 
                          << lvl << ' ';
                    m_out.write(log.payload, log.payload_len);
                    m_out << '\n';
                }
                
                m_out.flush();
            }
            m_out.flush();
        }

    };
} // namespace hft
#define LOGGER_H
#endif
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <new>
#include <atomic>
#include <array>

namespace hft
{
    template <typename T, std::size_t Size>
    class SPSCRingBuffer
    {
        static_assert(Size && ((Size &(Size - 1)) == 0),
                      "Size must be a power of two");
        static constexpr std::size_t CacheLineSize = std::hardware_destructive_interference_size;

    public:
        [[nodiscard]]
        bool Push(const T item) noexcept
        {
            const auto write_index = m_producer_index.load(std::memory_order_relaxed);
            const auto next_write = (write_index + 1) & (Size - 1);
            const auto read_index = m_consumer_index.load(std::memory_order_acquire);
            
            if (next_write == read_index)
            {
                return false;
            }
            
            m_data[write_index] = item;
            m_producer_index.store(next_write, std::memory_order_release);
            return true;
        }

        [[nodiscard]] 
        bool Pop(T& buffer) noexcept
        {
            const auto read_index = m_consumer_index.load(std::memory_order_relaxed);
            const auto write_index = m_producer_index.load(std::memory_order_acquire);
            
            if (read_index == write_index)
            {
                return false;
            }

            buffer = m_data[read_index];
            const auto next_read = (read_index + 1) & (Size - 1);
            m_consumer_index.store(next_read, std::memory_order_release);
            return true;    
        }

        auto Peek(const uint64_t index) const noexcept
        {
            return m_data[index];
        }

        bool Empty() const noexcept
        {
            return m_consumer_index.load(std::memory_order_acquire) ==
                m_producer_index.load(std::memory_order_acquire);
        }

        // TODO(vss): add emplace(), capacity(), size()
    private:
        std::array<T, Size> m_data;
        alignas(CacheLineSize) std::atomic<uint64_t> m_producer_index{ 0 };
        alignas(CacheLineSize) std::atomic<uint64_t> m_consumer_index{ 0 };
    };

} // namespace hft

#endif
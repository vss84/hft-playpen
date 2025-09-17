#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <new>
#include <atomic>
#include <memory>
#include <array>
#include <concepts>
#include <type_traits>
#include <limits>

namespace hft
{
#undef max

    template <typename Alloc> 
    concept has_allocate_at_least = 
        requires(Alloc& a, std::size_t n) 
    { 
        { a.allocate_at_least(n) }; 
    };

    template <size_t N>
    concept power_of_two = (N > 0) && ((N & (N - 1)) == 0);

    template <typename T>
    concept no_throw_destructible = std::is_nothrow_destructible_v<T>;

    template <typename T, size_t Capacity, typename Allocator = std::allocator<T>> 
        requires power_of_two<Capacity> 
    class SPSCRingBuffer
    {
        using AllocTraits = std::allocator_traits<Allocator>;
        static constexpr size_t CacheLineSize = std::hardware_destructive_interference_size;
        static constexpr size_t Padding = (CacheLineSize - 1) / sizeof(T) + 1;
        static constexpr size_t MaxSize = std::numeric_limits<size_t>::max() - (2 * Padding);

    public:
        explicit SPSCRingBuffer(const Allocator &alloc = Allocator()) 
            : m_alloc(alloc)
        {
            static_assert(power_of_two<Capacity>, "Capacity must be a power of two");
            static_assert(Capacity > 0, "Capacity must be greater than 0");
            static_assert(Capacity <= MaxSize, "Capacity must be less or equal to MaxSize");

            if constexpr (has_allocate_at_least<Allocator>)
            {
                auto result = m_alloc.allocate_at_least(Capacity + 2 * Padding);
                m_data = result.ptr;
            }
            else
            {
                m_data = AllocTraits::allocate(m_alloc, Capacity + 2 * Padding);
            }

            static_assert(alignof(SPSCRingBuffer) == CacheLineSize, "!= CacheLineSize");
            static_assert(sizeof(SPSCRingBuffer) >= 3 * CacheLineSize, "< CacheLineSize");
        }

        ~SPSCRingBuffer()
        {
            while (TryPop());
            AllocTraits::deallocate(m_alloc, m_data,
                                    Capacity + 2 * Padding);
        }

        SPSCRingBuffer(const SPSCRingBuffer &) = delete;
        SPSCRingBuffer& operator=(const SPSCRingBuffer &) = delete;
        
        SPSCRingBuffer(SPSCRingBuffer &&) = delete;
        SPSCRingBuffer& operator=(SPSCRingBuffer &&) = delete;

        template <typename... Args>
        requires std::constructible_from<T, Args&&...>
        [[nodiscard]] bool TryEmplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
        {
            const auto write_index = m_producer_index.load(std::memory_order_relaxed);
            const auto next_write = (write_index + 1) & (Capacity - 1);
            const auto read_index = m_consumer_index.load(std::memory_order_acquire);

            if (next_write == read_index)
            {
                return false;
            }

            new(&m_data[write_index + Padding]) T(std::forward<Args>(args)...);

            m_producer_index.store(next_write, std::memory_order_release);

            return true;
        }
        
        template <typename Value>
        requires std::constructible_from<T, Value&&>
        [[nodiscard]] bool TryPush(Value&& value) noexcept(std::is_nothrow_constructible_v<T, Value&&>)
        {
            return TryEmplace(std::forward<Value>(value));
        }

        [[nodiscard]] bool TryPop() noexcept 
        requires no_throw_destructible<T>
        {
            const auto read_index = m_consumer_index.load(std::memory_order_relaxed);
            const auto write_index = m_producer_index.load(std::memory_order_acquire);
            if (read_index == write_index)
            {
                return false;
            }
            m_data[read_index + Padding].~T();
            const auto next_read = (read_index + 1) & (Capacity - 1);
            m_consumer_index.store(next_read, std::memory_order_release);
            
            return true;
        }

        [[nodiscard]] T *Peek() noexcept
        {
            const auto read_index = m_consumer_index.load(std::memory_order_relaxed);
            const auto write_index = m_producer_index.load(std::memory_order_acquire);
            if (read_index == write_index)
            {
                return nullptr;
            }

            return &m_data[read_index + Padding];
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            std::ptrdiff_t offset =
                m_producer_index.load(std::memory_order_acquire) -
                m_consumer_index.load(std::memory_order_acquire);

            if (0 > offset)
            {
                offset += Capacity;
            }

            return static_cast<size_t>(offset);
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            return m_producer_index.load(std::memory_order_acquire) ==
                   m_consumer_index.load(std::memory_order_acquire);
        }

        [[nodiscard]] constexpr size_t GetCapacity() const noexcept { return Capacity; }

    private:
        T* m_data;

#ifdef _MSC_VER
        Allocator m_alloc [[msvc::no_unique_address]];
#else
        Allocator m_alloc [[no_unique_address]];
#endif

        alignas(CacheLineSize) std::atomic<uint64_t> m_producer_index{ 0 };
        alignas(CacheLineSize) std::atomic<uint64_t> m_consumer_index{ 0 };
    };

} // namespace hft

#endif
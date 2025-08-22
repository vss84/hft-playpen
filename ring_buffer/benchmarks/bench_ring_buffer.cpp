#include <benchmark/benchmark.h>
#include <windows.h>
#include <stdexcept>
#include <thread>
#include <iostream>

#include "ring_buffer.h"

using namespace hft;

static void PinThread(int cpu) 
{
    if (cpu < 0) 
    {
        return;
    }
    HANDLE thread_handle = GetCurrentThread();

    DWORD_PTR mask = 1ull << cpu;

    if (SetThreadAffinityMask(thread_handle, mask) == 0)
    {
        std::cerr << "SetThreadAffinityMask failed with error "
            << GetLastError() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

constexpr auto cpu1 = 0;
constexpr auto cpu2 = 1;

static void BM_SPSC_RingBuffer(benchmark::State &state)
{
    SPSCRingBuffer<int, 131072> queue;
    auto value = 0;
    
    auto t = std::jthread([&] {
        PinThread(cpu1);
        for (int expected = 0; ; ++expected)
        {
            int val;
            while (!queue.Pop(val))
            {
                ;
            }
            benchmark::DoNotOptimize(val);

            if (val == -1) break;
            if (val != expected)
            {
                throw std::runtime_error("invalid value");
            }
        }
    });

    PinThread(cpu2);
    for (auto _ : state)
    {
        while (!queue.Push(value))
        {
            ;
        }
        ++value;
    }
    
    state.counters["ops/s"] = benchmark::Counter(double(value), benchmark::Counter::kIsRate);
    state.PauseTiming();
    queue.Push(-1);
}
BENCHMARK(BM_SPSC_RingBuffer);
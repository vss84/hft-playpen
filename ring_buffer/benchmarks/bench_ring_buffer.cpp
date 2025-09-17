#include <benchmark/benchmark.h>
#include <windows.h>
#include <stdexcept>
#include <thread>
#include <iostream>

#include "ring_buffer/ring_buffer.h"

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

#include <gtest/gtest.h>
#include <thread>

#include "ring_buffer/ring_buffer.h"

using namespace hft;

TEST(SPSCRingBuffer, BasicTest)
{
    SPSCRingBuffer<int, 16> rb;

    for (int i = 0; i < 15; ++i)
    {
        ASSERT_TRUE(rb.Push(i));
    }

    ASSERT_FALSE(rb.Push(99));

    for (int i = 0; i < 15; ++i)
    {
        int buffer;
        auto result = rb.Pop(buffer);
        ASSERT_TRUE(result);
        EXPECT_EQ(buffer, i);
    }

    int temp;
    ASSERT_FALSE(rb.Pop(temp));
}

TEST(SPSCRingBuffer, BasicTest2)
{

    SPSCRingBuffer<int, 1024> rb;
    constexpr int items = 10'000'000;

    std::thread producer(
        [&]
        {

            for (int i = 0; i < items; ++i)
            {
                while (!rb.Push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

    std::thread consumer(
        [&] 
        { 
            for (int expected = 0; expected < items; ++expected) 
            {  
                int value;
                while (!rb.Pop(value)) 
                {
                    std::this_thread::yield();
                }
                EXPECT_EQ(value, expected);
            }
        });

    producer.join();
    consumer.join();
}
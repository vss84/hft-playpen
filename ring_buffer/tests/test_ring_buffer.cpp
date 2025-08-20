#include <gtest/gtest.h>

#include "ring_buffer.h"
#include <iostream>

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
        std::cout << "pre pop: " << rb.Peek(i);

        auto result = rb.Pop(buffer);
        std::cout << " post pop: " << rb.Peek(i) << "\n";
        std::cout << " result: " << result << "\n";

        ASSERT_TRUE(result);
        EXPECT_EQ(buffer, i);
    }

    int temp;
    ASSERT_FALSE(rb.Pop(temp));
}
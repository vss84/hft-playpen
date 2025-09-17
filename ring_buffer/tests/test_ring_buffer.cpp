#include <gtest/gtest.h>
#include <thread>
#include <iostream>
#include "ring_buffer/ring_buffer.h"

using namespace hft;

TEST(SPSCRingBuffer, FunctionalityTest)
{
    SPSCRingBuffer<int, 16> rb;

    ASSERT_EQ(rb.Peek(), nullptr);
    ASSERT_EQ(rb.Size(), 0);
    ASSERT_TRUE(rb.Empty());
    ASSERT_EQ(rb.GetCapacity(), 16);

    for (int i = 0; i < 15; ++i)
    {
        rb.TryEmplace(i);
    }
    
    ASSERT_NE(rb.Peek(), nullptr);
    ASSERT_EQ(rb.Size(), 15);
    ASSERT_FALSE(rb.Empty());
    
    ASSERT_FALSE(rb.TryEmplace());
    rb.TryPop();
    ASSERT_EQ(rb.Size(), 14);
    rb.TryPop();
    ASSERT_TRUE(rb.TryEmplace());

}

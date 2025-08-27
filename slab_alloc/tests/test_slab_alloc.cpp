#include <gtest/gtest.h>
#include "slab_alloc.h"

using namespace hft;

TEST(SlabAllocator, SingleAllocFree)
{
    SlabAlloc a;
    void *p = a.Allocate(16);
    ASSERT_NE(p, nullptr);
    a.Deallocate(p);
}

TEST(SlabAllocator, MinSizeRounding)
{
    SlabAlloc a;
    void *p = a.Allocate(1);
    void *q = a.Allocate(sizeof(void *));
    
    ASSERT_NE(p, nullptr);
    ASSERT_NE(q, nullptr);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % sizeof(void *), 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(q) % sizeof(void *), 0u);

#ifdef SLAB_ALLOC_DEBUG
    EXPECT_EQ(a.DebugSlabHeaderFromPtr(p)->owner->obj_size,
              a.DebugSlabHeaderFromPtr(q)->owner->obj_size);
#endif
}

TEST(SlabAllocator, SlabResizing)
{
    SlabAlloc a;
    const size_t req = 64;
    void *p = a.Allocate(req);
    ASSERT_NE(p, nullptr);

#ifdef SLAB_ALLOC_DEBUG
    size_t slots = a.DebugSlotsPerSlab(req);
    ASSERT_GT(slots, 0u);
    
    std::vector<void *> allocated;
    allocated.reserve(slots + 2);
    allocated.push_back(p);

    for (size_t i = 0; i < slots - 1; ++i)
    {
        void *q = a.Allocate(req);
        ASSERT_NE(q, nullptr);
        allocated.push_back(q);
    }

    void *extra = a.Allocate(req);
    ASSERT_NE(extra, nullptr);
    allocated.push_back(extra);

    EXPECT_GE(a.DebugSlabsInCache(a.DebugAlignedSize(req)), 2u);

    for (void *p : allocated)
    {
        a.Deallocate(p);
    }
#else
    GTEST_SKIP() << "Enable SLAB_ALLOC_DEBUG to run deterministic slab tests";
#endif
}

TEST(SlabAllocator, SlotReuse)
{
    SlabAlloc a;
    void *p = a.Allocate(1);
    ASSERT_NE(p, nullptr);
    auto address = reinterpret_cast<uintptr_t>(p);
    a.Deallocate(p);
    void *q = a.Allocate(1);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(address, reinterpret_cast<uintptr_t>(q));
    a.Deallocate(q);
}

TEST(SlabAllocator, DoubleFree)
{
    GTEST_SKIP() << "TODO: Double-free protection not yet implemented. "
                 << "Currently causes silent corruption.";
}

TEST(SlabAllocator, LargeAllocation)
{
    SlabAlloc a;
    
    void *p = a.Allocate(4096);
    EXPECT_EQ(p, nullptr);

#ifdef SLAB_ALLOC_DEBUG
    void *q = a.Allocate(2048);
    ASSERT_NE(q, nullptr);
    
    EXPECT_EQ(a.DebugSlabsInCache(a.DebugAlignedSize(2048)), 1u);
    a.Deallocate(q);
#endif
}

TEST(SlabAllocator, ZeroAllocation)
{
    SlabAlloc a;
    void *p = a.Allocate(0);
    EXPECT_EQ(p, nullptr);
}

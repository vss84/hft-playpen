#include <gtest/gtest.h>
#include "orderbook/orderbook.h"  // includes types.h

using namespace hft;

static std::unique_ptr<Order> NewOrderPtr(OrderId id, Side side, Price price, Quantity qty) 
{
    auto order = std::make_unique<Order>();
    order->id = id;
    order->side = side;
    order->price = price;
    order->quantity = qty;
    order->filled_qty = 0;
    order->status = OrderStatus::ACTIVE;
    return order;
}

TEST(OrderbookBasic, AddGetRemoveBestPrice) 
{
    Orderbook ob;

    ob.AddOrder(NewOrderPtr(1, Side::BUY, 101.0, 10));
    EXPECT_TRUE(ob.HasBids());
    auto best_bid = ob.BestBid();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_DOUBLE_EQ(*best_bid, 101.0);

    ob.AddOrder(NewOrderPtr(2, Side::SELL, 102.0, 5));
    EXPECT_TRUE(ob.HasAsks());
    auto best_ask = ob.BestAsk();
    ASSERT_TRUE(best_ask.has_value());
    EXPECT_DOUBLE_EQ(*best_ask, 102.0);

    ob.RemoveOrder(1);
    EXPECT_FALSE(ob.HasBids());
}

TEST(OrderbookFIFO, SamePriceMaintainsFIFO) 
{
    Orderbook ob;

    ob.AddOrder(NewOrderPtr(10, Side::BUY, 100.0, 7));
    ob.AddOrder(NewOrderPtr(11, Side::BUY, 100.0, 3));

    Order *best_order = ob.GetBestOrder(Side::BUY);
    ASSERT_NE(best_order, nullptr);
    EXPECT_EQ(best_order->id, 10);

    ob.RemoveOrder(10);
    best_order = ob.GetBestOrder(Side::BUY);
    ASSERT_NE(best_order, nullptr);
    EXPECT_EQ(best_order->id, 11);

    ob.RemoveOrder(11);
    EXPECT_FALSE(ob.HasBids());
}

TEST(OrderbookSnapshot, SnapshotTopDepth) 
{
    Orderbook ob;
    ob.AddOrder(NewOrderPtr(1, Side::BUY, 110.0, 2));
    ob.AddOrder(NewOrderPtr(2, Side::BUY, 109.0, 4));
    ob.AddOrder(NewOrderPtr(3, Side::SELL, 120.0, 1));
    ob.AddOrder(NewOrderPtr(4, Side::SELL, 121.0, 5));

    auto snap = ob.SnapshotTop(2);

    EXPECT_EQ(snap.bids.size(), 2u);
    EXPECT_EQ(snap.asks.size(), 2u);

    EXPECT_DOUBLE_EQ(snap.bids[0].price, 110.0);
    EXPECT_DOUBLE_EQ(snap.asks[0].price, 120.0);
}

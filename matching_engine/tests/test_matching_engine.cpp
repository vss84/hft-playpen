#include <gtest/gtest.h>
#include "matching_engine/matching_engine.h" 

using namespace hft;

static OrderRequest MakeNewOrder(Side side, Price price, Quantity qty,
                                 OrderType type = OrderType::LIMIT,
                                 TimeInForce tif = TimeInForce::GTC,
                                 OrderId explicit_id = 0) 
{
    OrderRequest req{ };
    req.type = RequestType::NEW_ORDER;
    req.order = Order{};
    req.order.side = side;
    req.order.price = price;
    req.order.quantity = qty;
    req.order.type = type;
    req.order.tif = tif;
    if (explicit_id != 0) req.order.id = explicit_id;
    req.timestamp_ns = 0;
    return req;
}

static OrderRequest MakeCancelRequest(OrderId target_id) 
{
    OrderRequest req{ };
    req.type = RequestType::CANCEL_ORDER;
    req.order_id_to_cancel = target_id;
    req.timestamp_ns = 0;
    return req;
}

TEST(MatchingEngineBasic, FullMatchProducesTrade) 
{
    MatchingEngine engine;

    auto maker = MakeNewOrder(Side::SELL, 100.0, 10);
    engine.ProcessOrderRequest(maker);

    auto taker = MakeNewOrder(Side::BUY, 100.0, 10);
    engine.ProcessOrderRequest(taker);

    auto trades = engine.GetAndClearTrades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 10u);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
}

TEST(MatchingEngineBasic, PartialThenFillProducesTwoTrades) 
{
    MatchingEngine engine;

    auto maker = MakeNewOrder(Side::SELL, 50.0, 10);
    engine.ProcessOrderRequest(maker);

    auto taker1 = MakeNewOrder(Side::BUY, 50.0, 6);
    engine.ProcessOrderRequest(taker1);
    auto trades1 = engine.GetAndClearTrades();
    ASSERT_EQ(trades1.size(), 1u);
    EXPECT_EQ(trades1[0].quantity, 6u);

    auto taker2 = MakeNewOrder(Side::BUY, 50.0, 4);
    engine.ProcessOrderRequest(taker2);
    auto trades2 = engine.GetAndClearTrades();
    ASSERT_EQ(trades2.size(), 1u);
    EXPECT_EQ(trades2[0].quantity, 4u);
}

TEST(MatchingEngineMarketOrder, MarketTakerConsumesBest) 
{
    MatchingEngine engine;

    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 105.0, 3));
    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 106.0, 5));

    auto market = MakeNewOrder(Side::BUY, 0.0, 3, OrderType::MARKET);
    engine.ProcessOrderRequest(market);

    auto trades = engine.GetAndClearTrades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_DOUBLE_EQ(trades[0].price, 105.0);
    EXPECT_EQ(trades[0].quantity, 3u);
}



TEST(MatchingEngineExtra, CancelRemovesRestingOrder) 
{
    MatchingEngine engine;

    auto maker = MakeNewOrder(Side::SELL, 100.0, 10, OrderType::LIMIT,
                              TimeInForce::GTC, /*id=*/200);

    engine.ProcessOrderRequest(maker);

    auto cancel = MakeCancelRequest(200);
    engine.ProcessOrderRequest(cancel);

    auto taker = MakeNewOrder(Side::BUY, 100.0, 10);
    engine.ProcessOrderRequest(taker);

    auto trades = engine.GetAndClearTrades();
    EXPECT_EQ(trades.size(), 0u);
}

TEST(MatchingEngineExtra, FOKRejectedWhenInsufficientLiquidity) 
{
    MatchingEngine engine;

    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 100.0, 5));

    OrderRequest fok_req = MakeNewOrder(Side::BUY, 100.0, 10);
    fok_req.order.tif = TimeInForce::FOK;
    engine.ProcessOrderRequest(fok_req);

    auto trades = engine.GetAndClearTrades();
    EXPECT_EQ(trades.size(), 0u);

    auto market_taker = MakeNewOrder(Side::BUY, 0.0, 5, OrderType::MARKET);

    engine.ProcessOrderRequest(market_taker);
    auto trades2 = engine.GetAndClearTrades();
    ASSERT_EQ(trades2.size(), 1u);
    EXPECT_EQ(trades2[0].quantity, 5u);
    EXPECT_DOUBLE_EQ(trades2[0].price, 100.0);
}

TEST(MatchingEngineExtra, FIFOWithinLevel) 
{
    MatchingEngine engine;

    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 50.0, 7, OrderType::LIMIT,
                                            TimeInForce::GTC, /*id=*/100));

    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 50.0, 3,  OrderType::LIMIT, 
                                            TimeInForce::GTC, /*id=*/101));

    engine.ProcessOrderRequest(MakeNewOrder(Side::BUY, 50.0, 8));

    auto trades = engine.GetAndClearTrades();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_order_id, 100u);
    EXPECT_EQ(trades[0].quantity, 7u);
    EXPECT_EQ(trades[1].maker_order_id, 101u);
    EXPECT_EQ(trades[1].quantity, 1u);
}

TEST(MatchingEngineExtra, MultiLevelMatching) 
{
    MatchingEngine engine;

    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 105.0, 3, OrderType::LIMIT,
                                            TimeInForce::GTC, /*id=*/300));

    engine.ProcessOrderRequest(MakeNewOrder(Side::SELL, 106.0, 5, OrderType::LIMIT, 
                                            TimeInForce::GTC, /*id=*/301));

    engine.ProcessOrderRequest(MakeNewOrder(Side::BUY, 0.0, 5, OrderType::MARKET));

    auto trades = engine.GetAndClearTrades();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_DOUBLE_EQ(trades[0].price, 105.0);
    EXPECT_EQ(trades[0].quantity, 3u);
    EXPECT_DOUBLE_EQ(trades[1].price, 106.0);
    EXPECT_EQ(trades[1].quantity, 2u);
}
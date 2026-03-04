#include <gtest/gtest.h>
#include <order_matcher/order.h>
#include <order_matcher/order_book.h>
#include <string>
#include <queue>

using namespace order_matcher;

TEST(OrderMatcher, MarketOrderBuyMatchesLimitSell)
{
    OrderBook book("TEST");
    Order limitSell("1", "TEST", "Seller", "Server", OrderSide::SELL, OrderType::LIMIT, 100.0, 10);
    book.insert(limitSell);

    Order marketBuy("2", "TEST", "Buyer", "Server", OrderSide::BUY, OrderType::MARKET, 0.0, 5);
    book.insert(marketBuy);

    std::queue<Order> processedOrders;
    book.processMatching(processedOrders);

    EXPECT_EQ(2, processedOrders.size());
    Order first = processedOrders.front();
    processedOrders.pop();
    Order second = processedOrders.front();

    // Check execution
    EXPECT_EQ(5, first.getExecutedQuantity());
    EXPECT_EQ(100.0, first.getLastExecutedPrice());
    EXPECT_EQ(5, second.getExecutedQuantity());
    EXPECT_EQ(100.0, second.getLastExecutedPrice());
}

TEST(OrderMatcher, MarketOrderSellMatchesLimitBuy)
{
    OrderBook book("TEST");
    Order limitBuy("1", "TEST", "Buyer", "Server", OrderSide::BUY, OrderType::LIMIT, 100.0, 10);
    book.insert(limitBuy);

    Order marketSell("2", "TEST", "Seller", "Server", OrderSide::SELL, OrderType::MARKET, 0.0, 5);
    book.insert(marketSell);

    std::queue<Order> processedOrders;
    book.processMatching(processedOrders);

    EXPECT_EQ(2, processedOrders.size());
    Order first = processedOrders.front();
    processedOrders.pop();
    Order second = processedOrders.front();

    EXPECT_EQ(5, first.getExecutedQuantity());
    EXPECT_EQ(100.0, first.getLastExecutedPrice());
}

TEST(OrderMatcher, MarketOrderPriority)
{
    OrderBook book("TEST");
    // Limit Buy at 100
    Order limitBuy("1", "TEST", "Buyer1", "Server", OrderSide::BUY, OrderType::LIMIT, 100.0, 10);
    book.insert(limitBuy);

    // Market Buy (should go before Limit Buy)
    Order marketBuy("2", "TEST", "Buyer2", "Server", OrderSide::BUY, OrderType::MARKET, 0.0, 5);
    book.insert(marketBuy);

    // Limit Sell at 90 (matches both)
    Order limitSell("3", "TEST", "Seller", "Server", OrderSide::SELL, OrderType::LIMIT, 90.0, 5);
    book.insert(limitSell);

    std::queue<Order> processedOrders;
    book.processMatching(processedOrders);

    // Seller should match with Market Buy first
    EXPECT_EQ(2, processedOrders.size());
    Order first = processedOrders.front();
    EXPECT_EQ("2", first.getClientID()); // Market Buy client ID
}

TEST(OrderMatcher, MarketVsMarketUsesLastTradePrice)
{
    OrderBook book("TEST");
    
    // First establish a last trade price
    Order limitBuy("1", "TEST", "B1", "S", OrderSide::BUY, OrderType::LIMIT, 105.0, 10);
    Order limitSell("2", "TEST", "S1", "S", OrderSide::SELL, OrderType::LIMIT, 105.0, 10);
    book.insert(limitBuy);
    book.insert(limitSell);
    std::queue<Order> p1;
    book.processMatching(p1);
    
    // Now match Market vs Market
    Order mBuy("3", "TEST", "B2", "S", OrderSide::BUY, OrderType::MARKET, 0.0, 5);
    Order mSell("4", "TEST", "S2", "S", OrderSide::SELL, OrderType::MARKET, 0.0, 5);
    book.insert(mBuy);
    book.insert(mSell);
    
    std::queue<Order> p2;
    book.processMatching(p2);
    
    EXPECT_EQ(2, p2.size());
    EXPECT_EQ(105.0, p2.front().getLastExecutedPrice());
}

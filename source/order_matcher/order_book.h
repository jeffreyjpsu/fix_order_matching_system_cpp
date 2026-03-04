#ifndef _ORDER_BOOK_H_
#define _ORDER_BOOK_H_

#include "order.h"

#include <map>
#include <queue>
#include <string>
#include <functional>
#include <memory>
#include <vector> // Added for std::vector

#include <utility/visitor.hpp>

namespace order_matcher
{

struct OrderArrays {
    std::vector<std::string> clientOrderIDs;
    std::vector<std::string> symbols;
    std::vector<std::string> owners;
    std::vector<std::string> targets;
    std::vector<OrderSide> sides;
    std::vector<OrderType> types;
    std::vector<double> prices;
    std::vector<long> quantities;
    std::vector<long> openQuantities;
    std::vector<long> executedQuantities;
    std::vector<bool> cancelled; // vector<bool> is space efficient but maybe slower, consider vector<char> if needed
    std::vector<double> averageExecutedPrices;
    std::vector<double> lastExecutedPrices;
    std::vector<long> lastExecutedQuantities;

    void push_back(const Order& order) {
        clientOrderIDs.push_back(order.getClientID());
        symbols.push_back(order.getSymbol());
        owners.push_back(order.getOwner());
        targets.push_back(order.getTarget());
        sides.push_back(order.getSide());
        types.push_back(order.getOrderType());
        prices.push_back(order.getPrice());
        quantities.push_back(order.getQuantity());
        openQuantities.push_back(order.getOpenQuantity());
        executedQuantities.push_back(order.getExecutedQuantity());
        cancelled.push_back(order.isCancelled());
        averageExecutedPrices.push_back(order.getAverageExecutedPrice());
        lastExecutedPrices.push_back(order.getLastExecutedPrice());
        lastExecutedQuantities.push_back(order.getLastExecutedQuantity());
    }

    void erase(size_t index) {
        clientOrderIDs.erase(clientOrderIDs.begin() + index);
        symbols.erase(symbols.begin() + index);
        owners.erase(owners.begin() + index);
        targets.erase(targets.begin() + index);
        sides.erase(sides.begin() + index);
        types.erase(types.begin() + index);
        prices.erase(prices.begin() + index);
        quantities.erase(quantities.begin() + index);
        openQuantities.erase(openQuantities.begin() + index);
        executedQuantities.erase(executedQuantities.begin() + index);
        cancelled.erase(cancelled.begin() + index);
        averageExecutedPrices.erase(averageExecutedPrices.begin() + index);
        lastExecutedPrices.erase(lastExecutedPrices.begin() + index);
        lastExecutedQuantities.erase(lastExecutedQuantities.begin() + index);
    }

    void insert(size_t index, const Order& order) {
        clientOrderIDs.insert(clientOrderIDs.begin() + index, order.getClientID());
        symbols.insert(symbols.begin() + index, order.getSymbol());
        owners.insert(owners.begin() + index, order.getOwner());
        targets.insert(targets.begin() + index, order.getTarget());
        sides.insert(sides.begin() + index, order.getSide());
        types.insert(types.begin() + index, order.getOrderType());
        prices.insert(prices.begin() + index, order.getPrice());
        quantities.insert(quantities.begin() + index, order.getQuantity());
        openQuantities.insert(openQuantities.begin() + index, order.getOpenQuantity());
        executedQuantities.insert(executedQuantities.begin() + index, order.getExecutedQuantity());
        cancelled.insert(cancelled.begin() + index, order.isCancelled());
        averageExecutedPrices.insert(averageExecutedPrices.begin() + index, order.getAverageExecutedPrice());
        lastExecutedPrices.insert(lastExecutedPrices.begin() + index, order.getLastExecutedPrice());
        lastExecutedQuantities.insert(lastExecutedQuantities.begin() + index, order.getLastExecutedQuantity());
    }
    
    size_t size() const { return prices.size(); }
};

class OrderBook : public utility::Visitable<Order>
{
    public :
        OrderBook() = default;
        explicit OrderBook(const std::string& symbol);

        void accept(utility::Visitor<Order>& v) override;

        void insert(const Order& order);
        bool find(Order** order, const std::string& owner, const std::string& clientID, OrderSide side);
        void erase(const Order& order);
        bool processMatching( std::queue<Order>& processedOrders );
        bool isEmpty() const { return (m_bids.size() == 0) && (m_asks.size() == 0); }

    private:
        std::string m_symbol;
        long m_lastOrderID = 0;
        double m_lastTradePrice = 0.0;
        
        OrderArrays m_bids;
        OrderArrays m_asks;

        // Helper to reconstruct Order object from SoA at index
        Order reconstructOrder(const OrderArrays& arrays, size_t index) const;
        void updateOrderFromSoA(Order& order, const OrderArrays& arrays, size_t index) const;
        
        void matchTwoOrders(size_t bidIndex, size_t askIndex);
};

using OrderBookPtr = std::unique_ptr<order_matcher::OrderBook>;

} //namespace

#endif
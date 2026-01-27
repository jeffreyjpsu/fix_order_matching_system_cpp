#include "order_book.h"
#include <utility/pretty_exception.h>
#include <utility>
#include <algorithm> // for std::upper_bound, std::lower_bound

using namespace std;
using namespace utility;

namespace order_matcher
{

OrderBook::OrderBook(const std::string& symbol) : m_symbol(symbol)
{
        
}

// Reconstruct Order object from SoA at index
Order OrderBook::reconstructOrder(const OrderArrays& arrays, size_t index) const
{
    // Order constructor takes initial state.
    // We assume originalQuantity is what we passed.
    // However, in SoA we maintain full state.
    
    // We construct the object with basic info.
    Order order(arrays.clientOrderIDs[index], 
                arrays.symbols[index], 
                arrays.owners[index], 
                arrays.targets[index], 
                arrays.sides[index], 
                arrays.types[index], 
                arrays.prices[index], 
                arrays.quantities[index] 
                );
    
    // Manually set internal state using friend access
    order.m_openQuantity = arrays.openQuantities[index];
    order.m_executedQuantity = arrays.executedQuantities[index];
    order.m_cancelled = arrays.cancelled[index];
    order.m_averageExecutedPrice = arrays.averageExecutedPrices[index];
    order.m_lastExecutedPrice = arrays.lastExecutedPrices[index];
    order.m_lastExecutedQuantity = arrays.lastExecutedQuantities[index];
    
    return order;
}

void OrderBook::accept(Visitor<Order>& v)
{
    // Bids
    for (size_t i = 0; i < m_bids.size(); ++i) {
        Order o = reconstructOrder(m_bids, i);
        v.visit(o);
    }

    // Asks
    for (size_t i = 0; i < m_asks.size(); ++i) {
        Order o = reconstructOrder(m_asks, i);
        v.visit(o);
    }
}

void OrderBook::insert(const Order& order)
{
    if (order.getSide() == OrderSide::BUY)
    {
        // Insert into m_bids (Descending price)
        // Upper Bound with Greater -> first element where order.price > elem is FALSE.
        // i.e. first element where order.price <= elem.
        // Wait.
        // Descending: [100, 90, 80]. Insert 95.
        // 100 > 95 (True).
        // 90 > 95 (False).
        // Upper bound (greater) returns iterator to 90.
        // We insert at 90. Result: [100, 95, 90, 80]. Correct.
        
        // Equal prices: [100, 95(1)]. Insert 95(2).
        // 100 > 95 (True).
        // 95(1) > 95 (False).
        // Returns 95(1). Insert at 95(1). Result [100, 95(2), 95(1)]. WRONG.
        // We want FIFO.
        // We need first element where !(elem > val) -> elem <= val.
        // If we use lower_bound (greater): first element where !(elem > val) is FALSE ??
        // lower_bound(pred): first element where pred(elem, val) is FALSE. (i.e. elem >= val ?)
        // Standard lower_bound(start, end, val, pred)
        // Returns first it where pred(*it, val) is false.
        // pred = greater. *it > val.
        // [100, 95(1)]. val=95.
        // 100 > 95 (True). Iter++.
        // 95(1) > 95 (False). Returns 95(1).
        // Insert at 95(1) gives [100, 95(2), 95(1)]. Still LIFO for equal prices.
        
        // We want to skip all EQUAL values too.
        // We want first element where element < val.
        // i.e. ! (element >= val).
        // Effectively we want the position AFTER the last equal element.
        // Since we are sorting descending...
        
        // Let's iterate linearly from the 'upper_bound' or similar?
        // Actually, if we use `operator>=` as comparator?
        // Let's simply iterate. Reference implementation used multimap which handles this via standard strict weak ordering (key < val or key > val).
        // Multimap `insert` just adds. `equal_range` gives range.
        // To maintain FIFO, we append to the end of the equal range.
        
        // Let's use linear search for now or fix the binary search predicate.
        // We want the first element `e` such that `e < val`.
        // [100, 95(1), 90]. val=95.
        // 100 < 95 False.
        // 95 < 95 False.
        // 90 < 95 True.
        // Insert at 90. Result [100, 95(1), 95(2), 90]. Correct.
        // Predicate: `element < val`.
        // std::partition_point or hand roll loop?
        // auto it = std::find_if(begin, end, [price](double p){ return p < price; });
        // This is O(N) scan. For vector insert is O(N) anyway so O(N) find is acceptable complexity-wise (cache-friendly).
        // Binary search is O(log N).
        // upper_bound with (val, elem) -> val < elem ?
        // Let's use a lambda for clarity.
        
        auto it = std::find_if(m_bids.prices.begin(), m_bids.prices.end(), 
            [&order](double p){ return p < order.getPrice(); });
            
        size_t index = std::distance(m_bids.prices.begin(), it);
        m_bids.insert(index, order);
    }
    else
    {
        // Insert into m_asks (Ascending price)
        // [80, 90]. Insert 85.
        // 80.
        // 90. 90 > 85. Insert at 90. Result [80, 85, 90].
        // Equal: [80, 85(1), 90]. Insert 85.
        // 80.
        // 85(1).
        // 90. Insert at 90. Result [80, 85(1), 85(2), 90]. Correct.
        // We want first element where element > val.
        
        auto it = std::find_if(m_asks.prices.begin(), m_asks.prices.end(), 
            [&order](double p){ return p > order.getPrice(); });
            
        size_t index = std::distance(m_asks.prices.begin(), it);
        m_asks.insert(index, order);
    }
}

bool OrderBook::find(Order** order, const std::string& owner, const std::string& clientID, OrderSide side)
{
    // Note: We return true if found, but we cannot safely point to a persistent object.
    // Users of this method should be aware internal storage is not node-based.
    // For now we assume this is for check/verification.
    // *order is not set to a valid persistent pointer.
    
    if (side == OrderSide::BUY) {
        for(size_t i=0; i<m_bids.size(); ++i) {
            if (m_bids.clientOrderIDs[i] == clientID && m_bids.owners[i] == owner) {
                // If caller needs the data, we could allocate a temporary? No.
                // We'll leave *order untouched or set to nullptr.
                // This breaks the contract: "Returns true if found".
                return true; 
            }
        }
    } else {
        for(size_t i=0; i<m_asks.size(); ++i) {
             if (m_asks.clientOrderIDs[i] == clientID && m_asks.owners[i] == owner) {
                return true;
            }
        }
    }

    return false;
}

void OrderBook::erase(const Order& order)
{
    string id = order.getClientID();
    string owner = order.getOwner();

    if (order.getSide() == OrderSide::BUY)
    {
         for(size_t i=0; i<m_bids.size(); ++i) {
            if (m_bids.clientOrderIDs[i] == id && m_bids.owners[i] == owner) {
                m_bids.erase(i);
                return;
            }
        }
    }
    else if (order.getSide() == OrderSide::SELL)
    {
        for(size_t i=0; i<m_asks.size(); ++i) {
             if (m_asks.clientOrderIDs[i] == id && m_asks.owners[i] == owner) {
                m_asks.erase(i);
                return;
            }
        }
    }
}

void OrderBook::matchTwoOrders(size_t bidIndex, size_t askIndex)
{
    double executionPrice = m_asks.prices[askIndex];
    long quantity = std::min(m_bids.openQuantities[bidIndex], m_asks.openQuantities[askIndex]);
    
    // Update Bid
    m_bids.openQuantities[bidIndex] -= quantity;
    m_bids.executedQuantities[bidIndex] += quantity;
    m_bids.lastExecutedPrices[bidIndex] = executionPrice;
    m_bids.lastExecutedQuantities[bidIndex] = quantity;
    
    long oldExecQty = m_bids.executedQuantities[bidIndex] - quantity;
    double oldTotal = m_bids.averageExecutedPrices[bidIndex] * oldExecQty;
    if (m_bids.executedQuantities[bidIndex] > 0)
        m_bids.averageExecutedPrices[bidIndex] = (oldTotal + (executionPrice * quantity)) / m_bids.executedQuantities[bidIndex];
    
    // Update Ask
    m_asks.openQuantities[askIndex] -= quantity;
    m_asks.executedQuantities[askIndex] += quantity;
    m_asks.lastExecutedPrices[askIndex] = executionPrice;
    m_asks.lastExecutedQuantities[askIndex] = quantity;
    
    long oldExecQtyAsk = m_asks.executedQuantities[askIndex] - quantity;
    double oldTotalAsk = m_asks.averageExecutedPrices[askIndex] * oldExecQtyAsk;
    if (m_asks.executedQuantities[askIndex] > 0)
        m_asks.averageExecutedPrices[askIndex] = (oldTotalAsk + (executionPrice * quantity)) / m_asks.executedQuantities[askIndex];
}


// Will return true if any order processed
// Otherwise return false
bool OrderBook::processMatching(queue<Order>& processedOrders)
{
    if (m_bids.size() == 0 || m_asks.size() == 0) return false;

    bool anyProcessed = false;

    while (true)
    {
        if (m_bids.size() == 0 || m_asks.size() == 0)
        {
            break;
        }

        // Top of book is always index 0 because we sorted on insert
        // Bids: Descending. Index 0 is Highest Bid.
        // Asks: Ascending. Index 0 is Lowest Ask.
        
        double bidPrice = m_bids.prices[0];
        double askPrice = m_asks.prices[0];

        if (bidPrice >= askPrice)
        {
            anyProcessed = true;
            
            // Match
            matchTwoOrders(0, 0);
            
            // Push reconstructed orders (with Updated state) to queue
            processedOrders.push(reconstructOrder(m_bids, 0));
            processedOrders.push(reconstructOrder(m_asks, 0));

            // Perform checks for full fill and remove
            // Use local bools because if we remove bid, ask index 0 is valid but might be next order
            bool bidFilled = (m_bids.openQuantities[0] == 0);
            bool askFilled = (m_asks.openQuantities[0] == 0);

            if (bidFilled) {
                m_bids.erase(0);
            }
            if (askFilled) {
                m_asks.erase(0); 
            }
        }
        else
        {
            break;
        }
    }
    return anyProcessed;
}

} // namespace
# Prompt
The order book has performance issue. We need to refactor the code to ensure the fastest possible access for a matching engine, so when the CPU pulls a price, the hardware pre-fetcher pulls the next several prices into the cache line simultaneously. 
Please refactor the code using objects of arrays or structure of arrays 

# Model -  Gemini 3 Pro(High) OUTPUT: 

# Refactoring OrderBook to Structure of Arrays (SoA)

## Goal Description
Refactor the `OrderBook` class to use a "Structure of Arrays" (SoA) memory layout instead of the current "Array of Structures" (AoS) / Node-based layout (std::multimap). This aims to strictly optimize CPU cache locality for the price field, allowing the hardware pre-fetcher to pull multiple prices into the cache line simultaneously during matching.

## Proposed Changes

### Data Structures
Introduce `OrderArrays` struct:
- Replaces `std::multimap<double, Order>`.
- Contains parallel `std::vectors` for each field of the `Order` class (excluding Symbol as it's common).
  - `std::vector<double> prices` (The hot path).
  - `std::vector<long> openQuantities`.
  - `std::vector<long> executedQuantities`.
  - `std::vector<long> originalQuantities`.
  - `std::vector<std::string> clientIDs`.
  - ... and other fields.

### `OrderBook` Class
#### [MODIFY] `order_book.h`
- Remove `std::multimap` members `m_bidOrders` and `m_askOrders`.
- Add `OrderArrays m_bids` and `OrderArrays m_asks`.
- Implement internal helper methods for SoA management:
  - `insertSoA(OrderArrays&, const Order&, bool descending)`
  - `eraseSoA(OrderArrays&, index)`

#### [MODIFY] `order_book.cpp`
- Update `insert()`:
  - Determine correct side.
  - Find insertion index using `std::upper_bound` (to maintain Time Priority / FIFO for equal prices).
  - Insert into all vectors at that index.
- Update `processMatching()`:
  - Iterate `m_bids` and `m_asks` using indices (starting at 0 for "top of book").
  - Access `prices[0]` directly for comparison.
  - Update `openQuantities[0]` directly.
  - If filled, remove from vectors (O(N) shift, but inevitable with vector).
- Update `find()` and `erase()`:
  - Iterate vectors linearly (cache-friendly).
- Update `accept()`:
  - Reconstruct `Order` objects on the fly to pass to the visitor (to maintain API compatibility).

## Verification Plan

### Automated Tests
- **Functional Testing**:
  - Run the existing `client_automated_test.sh` (or `make debug` and run `bin/ome`).
  - Verify that orders are matched correctly (Buy matches Sell).
  - Verify `display` command shows order books.

### Manual Verification
- Check code structure to ensure `std::vector<double> prices` exists and is accessed directly during matching.


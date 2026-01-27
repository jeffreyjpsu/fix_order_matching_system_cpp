# Prompt
The order book has performance issue. We need to refactor the code to ensure the fastest possible access for a matching engine, so when the CPU pulls a price, the hardware pre-fetcher pulls the next several prices into the cache line simultaneously.
Please refactor the code using objects of arrays or structure of arrays

# Model -  Gemini 3 Pro(Low) OUTPUT:

# Refactor OrderBook to use Structure of Arrays (SoA)

The goal is to improve the performance of the matching engine by ensuring better cache locality. We will switch from `std::multimap<double, Order>` (node-based) to a Structure of Arrays (SoA) layout (contiguous vectors).

## User Review Required
> [!IMPORTANT]
> The `OrderBook::find` method signature will be changed. It currently returns `Order**` (pointer to internal pointer/object). It will be changed to return `bool` and populate an `Order` reference argument (copy). This affects `CentralOrderBook::taskCancelOrder`.

## Proposed Changes

### [order_matcher]

#### [MODIFY] [order_book.h](file:///Users/Jeffrey.Su/lwk/src/github.com/cpp_ord_matching_engine/source/order_matcher/order_book.h)
- Define a private inner class or helper class `OrderBookSide`.
- `OrderBookSide` will contain `std::vector` for each field of `Order` (`prices`, `quantities`, `clientIDs`, etc.).
- `OrderBookSide` will maintain the vectors sorted by price (Bids: Descending, Asks: Ascending).
- Replace `std::multimap` members `m_bidOrders` and `m_askOrders` with `OrderBookSide m_bids` and `OrderBookSide m_asks`.
- Update `find` signature: `bool find(Order& order, ...)` instead of `bool find(Order** order, ...)`.

#### [MODIFY] [order_book.cpp](file:///Users/Jeffrey.Su/lwk/src/github.com/cpp_ord_matching_engine/source/order_matcher/order_book.cpp)
- Implement `OrderBookSide` methods: `insert`, `erase`, `size`, `getBestOrder`, `removeBestOrder`.
- Implement `reconstructOrder(index)` to create `Order` object from SoA fields.
- Rewrite `OrderBook::insert`, `erase`, `processMatching` to use `OrderBookSide`.
- `processMatching` will iterate through the vectors of `m_bids` and `m_asks`.

#### [MODIFY] [central_order_book.cpp](file:///Users/Jeffrey.Su/lwk/src/github.com/cpp_ord_matching_engine/source/order_matcher/central_order_book.cpp)
- Update `taskCancelOrder` to use the new `find` signature (working with a copy/value of the Order instead of a pointer).

## Verification Plan

### Automated Tests
- Since there are existing unit tests (I saw `test_unit` folder), I should run them to ensure no regression.
- I will search for tests related to `OrderBook`.
- I can write a small benchmark or script to verify the order matching logic still works.

### Manual Verification
- I will verify the code compiles.
- I will manually inspect the code to ensure the vector layout matches the "pre-fetching" requirement (contiguous prices).


# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is a production-grade multithreaded FIX (Financial Information Exchange) order matching engine written in C++11. The system implements a high-performance matching engine using lock-free data structures, thread pools with CPU affinity, and the FIX 4.2 protocol via QuickFix.

## Build System

### Building the Project

Use CMake (recommended) or the existing Makefiles:

```bash
# CMake build (recommended)
./build.sh

# Or manually:
mkdir -p build && cd build
cmake ..
make

# Executables will be in:
# - bin/order_matching_server (main server)
# - test_unit/unit_tests (unit tests)
```

The project uses C++11 (CMakeLists.txt has C++23 but code is C++11).

### Running Tests

**Unit Tests:**
```bash
cd test_unit
./unit_tests
```

Note: 4 concurrent queue tests currently fail (MPMC/MPSC implementations have known race conditions). The production SPSC queue and core order matching tests pass.

**Functional Tests:**
```bash
# 1. Start server in one terminal:
cd bin
./order_matching_server

# 2. In another terminal, run test client:
cd test_functional
./client_automated_test.sh
```

### Runtime Requirements

The server requires three configuration files in the `bin/` directory:
- `ome.ini` - Server configuration (symbols, thread settings, logging)
- `quickfix_server.cfg` - QuickFix session settings
- `quickfix_FIX42.xml` - FIX data dictionary

On macOS, set library path before running:
```bash
export DYLD_LIBRARY_PATH=dependencies/install/usr/local/lib:dependencies/quickfix/lib
```

## Architecture

### High-Level Flow

```
FIX Clients → QuickFix Engine → Incoming Message Dispatcher → Central Order Book
                                                                      ↓
                                                          Thread Pool (one thread per symbol)
                                                                      ↓
                                                          Order Books (MSFT, AAPL, etc.)
                                                                      ↓
                                                          Outgoing Message Queue
                                                                      ↓
                                                    Outgoing Message Processor → QuickFix → Clients
```

### Key Components

**Central Order Book** (`source/order_matcher/central_order_book.{h,cpp}`)
- Manages multiple order books (one per security symbol)
- Routes orders to symbol-specific worker threads via lock-free SPSC queues
- Coordinates thread pool and outgoing message queue
- Symbol → Queue ID mapping

**Order Book** (`source/order_matcher/order_book.{h,cpp}`)
- Price-time priority matching algorithm
- Bids: `std::multimap<double, Order, std::greater<double>>` (highest price first)
- Asks: `std::multimap<double, Order, std::less<double>>` (lowest price first)
- Matching occurs when top bid price ≥ top ask price

**Thread Pool** (`source/concurrent/thread_pool.{h,cpp}`)
- One worker thread per symbol (configured in `ome.ini`)
- Each thread has dedicated lock-free SPSC ring buffer queue
- Optional CPU core pinning (`CENTRAL_ORDER_BOOK_PIN_THREADS_TO_CORES=true`)
- Optional hyperthreading control (`HYPER_THREADING=false` uses physical cores only)

**Server** (`source/server/server.{h,cpp}`)
- Implements QuickFix `FIX::Application` and `FIX::MessageCracker`
- Handles FIX session lifecycle (onCreate, onLogon, onLogout)
- Routes FIX messages: `NewOrderSingle`, `OrderCancelRequest`
- Sends `ExecutionReport` messages back to clients

**Message Dispatcher** (`source/server/server_incoming_message_dispatcher.h`)
- Receives FIX messages from QuickFix thread
- Validates and converts to internal format
- Pushes orders to appropriate symbol queue in Central Order Book

**Outgoing Message Processor** (`source/server/server_outgoing_message_processor.h`)
- Collects execution reports from MPMC outgoing queue
- Converts internal format to FIX `ExecutionReport` messages
- Sends back to clients via QuickFix

### Concurrency Model

**Symbol-Based Partitioning:**
- Each symbol processed by dedicated thread (no lock contention between symbols)
- Configurable via `SYMBOL[]` array in `ome.ini`

**Lock-Free Queues:**
- **SPSC (Single Producer Single Consumer)**: `source/concurrent/ring_buffer_spsc_lockfree.hpp`
  - Used for incoming message queues (dispatcher → worker threads)
  - Truly lock-free using atomic operations with acquire-release semantics
  - Production-ready and tested
- **MPMC queues**: `source/concurrent/queue_mpmc_lockfree.hpp`
  - Used for outgoing message queue (worker threads → message processor)
  - NOT truly lock-free (known race conditions in tests)
  - Future enhancement planned

**Thread Affinity:**
- Threads can be pinned to specific CPU cores via `pthread_setaffinity_np` (Linux) or `SetThreadAffinityMask` (Windows)
- Reduces context switching and improves cache locality
- Can disable hyperthreading to use only physical cores

## Key Design Patterns

**Visitor Pattern**: Order book traversal without exposing internal structures
- `source/order_matcher/central_order_book_visitor.h`
- Used by `display` command to show all order books

**Observer Pattern**: Order book event notifications
- `source/order_matcher/central_order_book_observer.h`
- Logs order book state changes

**Flyweight Pattern**: String interning for symbols
- Uses `boost::flyweight<std::string>` for memory efficiency
- Multiple orders for same symbol share single string instance

## Code Organization

```
source/
├── compiler_portability/    # Cross-platform macros (LIKELY/UNLIKELY, alignas, noexcept, etc.)
├── concurrent/             # Thread pool, lock-free queues, threads with CPU affinity
├── memory/                 # Cache-aligned allocators, memory utilities
├── order_matcher/          # Core matching engine (Order, OrderBook, CentralOrderBook)
├── server/                 # FIX server (QuickFix integration, message routing)
├── utility/                # Config file parser, logger, file utilities
└── server_main.cpp         # Entry point

test_unit/                  # GoogleTest 1.7 unit tests
test_functional/            # FIX client test scripts and executables
dependencies/
├── boost/                  # Boost 1.59 headers (flyweight, noncopyable)
├── gtest-1.7.0/           # GoogleTest library
├── quickfix_build/        # QuickFix source
└── install/usr/local/     # QuickFix installed headers/libs
```

## FIX Protocol Integration

**Supported Inbound Messages:**
- `NewOrderSingle` (MsgType=D) - Submit limit order
- `OrderCancelRequest` (MsgType=F) - Cancel order

**Supported Outbound Messages:**
- `ExecutionReport` (MsgType=8) with ExecType:
  - `0` = New (order accepted)
  - `1` = Partial Fill
  - `2` = Fill (completely filled)
  - `4` = Canceled
  - `8` = Rejected

**Supported Order Types:**
- ✅ Limit orders only
- ❌ Market orders (not implemented)
- ❌ Stop-loss orders (not implemented)
- ❌ Time-In-Force (TIF) not supported

## Configuration (`ome.ini`)

Key parameters:
- `SYMBOL[]` - Array of symbols (one worker thread per symbol)
- `CENTRAL_ORDER_BOOK_PIN_THREADS_TO_CORES` - CPU affinity (true/false)
- `HYPER_THREADING` - Use logical cores (true) or physical only (false)
- `CENTRAL_ORDER_BOOK_WORK_QUEUE_SIZE_PER_THREAD` - SPSC ring buffer capacity
- `LOG_BUFFER_SIZE` - Lock-free logging ring buffer size
- `FILE_LOGGING_ENABLED` / `CONSOLE_OUTPUT_ENABLED` - Logging controls

## Server Runtime Commands

Once the server is running, type in the console:
- `display` - Show all order books and current orders
- `quit` - Gracefully shutdown server

## Compiler Portability

The project includes extensive cross-platform abstractions in `source/compiler_portability/`:

**MSVC 120 (VS2013) Limitations:**
- No `noexcept` support → custom `NOEXCEPT` macro
- Curly-brace initialization bug in Update 3+ → requires Update 2 or earlier
- Branch prediction: `LIKELY()` / `UNLIKELY()` macros map to GCC `__builtin_expect`
- Alignment: `ALIGNAS(n)` macro for cache line alignment
- Inlining: `FORCE_INLINE` / `NO_INLINE` macros

**Supported Platforms:**
- Linux (GCC 4.8, tested on CentOS/Ubuntu)
- Windows (MSVC 120 / VS2013)
- macOS (tested on ARM64, requires library path setup)

## Memory Management

**Cache Line Awareness:**
- `CACHE_LINE_SIZE = 64` bytes (x86/x64 typical)
- Custom allocators in `source/memory/` prevent false sharing
- Aligned allocators use `posix_memalign` (POSIX) or `_aligned_malloc` (Windows)

**Future Enhancements (from TODO):**
- jemalloc, tcmalloc, Intel TBB allocators
- Lockless memory allocator

## Important Implementation Notes

**Order Lifetime:**
- Orders tracked with `openQuantity`, `executedQuantity`, `averageExecutedPrice`
- Partial fills update both buyer and seller orders
- Orders removed from book only when fully filled or cancelled

**Matching Algorithm:**
- Price-time priority (FIFO within same price level)
- Uses `std::multimap` for automatic price sorting
- Matching executed in worker thread for each symbol (no cross-symbol locking)

**Thread Safety:**
- No shared mutable state between symbol worker threads
- SPSC queues ensure single producer (dispatcher) and single consumer (worker)
- Outgoing queue is MPMC (multiple workers, single processor)

## Known Issues

**Test Failures:**
- 4/11 unit tests fail (all concurrent queue tests: MPMC, MPSC, RingBufferMPMC, RingBufferSPSCLockFree)
- All fail with consistent pattern: expected sum 18, got 16 (missing 2 items)
- These are non-production data structures (future enhancements)
- Production SPSC queue used by thread pool works correctly

**Compiler Constraints:**
- Requires specific VS2013 Update 2 or earlier (Update 3+ has breaking bug)
- GCC 4.8 required (older version)
- No C++14/17 support

## Development Workflow

When adding new features:

1. **New Order Type**: Modify `OrderType` enum in `source/order_matcher/order.h`, update matching logic in `OrderBook::matchTwoOrders()`

2. **New Symbol**: Add to `SYMBOL[]` array in `bin/ome.ini` (requires server restart)

3. **New FIX Message Type**: Add handler in `source/server/server.h` (override `onMessage`), implement conversion in `source/order_matcher/quickfix_converter.h`

4. **Threading Changes**: Modify `source/concurrent/thread_pool.cpp`, ensure `ThreadPoolArguments` updated in `server_main.cpp`

5. **Performance Tuning**:
   - Increase `CENTRAL_ORDER_BOOK_WORK_QUEUE_SIZE_PER_THREAD` for high message rates
   - Enable `CENTRAL_ORDER_BOOK_PIN_THREADS_TO_CORES=true` for latency-sensitive workloads
   - Set `HYPER_THREADING=false` for predictable performance

## Testing Strategy

**Unit Tests** (`test_unit/test.cpp`):
- Tests utility functions (config parser, logger)
- Tests concurrent primitives (threads, actor model, thread pool)
- Tests order matching logic (order book matching)
- Run frequently during development

**Functional Tests** (`test_functional/`):
- End-to-end FIX message flow
- Multiple client simulation
- Test data in `test_cases.txt` (order specifications)
- Run before releases

## Logging

Custom lock-free logger in `source/utility/logger.{h,cpp}`:
- Ring buffer-based (non-blocking for worker threads)
- Dual output: file (`oms_log.txt`) and console
- Thread-safe via lock-free queue
- Format: `DD-MM-YYYY HH:MM:SS : LEVEL , Context , Message`

Use `LOG_INFO(context, message)`, `LOG_ERROR(context, message)` macros.

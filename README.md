# orderbook

A small single-symbol, single-threaded C++20 limit order book: price-time priority, GTC limit orders, cancel by id, partial fills, and trades at the resting order's price. Resting orders live in a preallocated pool of intrusive doubly-linked nodes, so a cancel
unlinks in O(1) without scanning the queue.

## Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && build/ob_tests
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DOB_SANITIZE=ON && cmake --build build-asan && build-asan/ob_tests
build/ob_tests cancel     # run only tests whose name contains "cancel"
```

## AI usage

Note that AI was used to help generate test for the book and help with CMake and Build setup. Nowhere else was it used. 
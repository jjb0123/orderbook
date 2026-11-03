#include <cstdint>
#include <random>
#include <vector>

#include "checked_book.hpp"
#include "harness.hpp"

using namespace test;

static void run_stream(std::uint64_t seed, std::size_t capacity, int commands) {
  std::mt19937_64 rng(seed);
  auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

  CheckedBook t(capacity);
  std::vector<ob::OrderId> live;
  ob::OrderId next_id = 1;

  for (int n = 0; n < commands; ++n) {
    const int r = pick(0, 99);
    if (r < 30 && !live.empty()) {
      const std::size_t k = pick(0, static_cast<int>(live.size()) - 1);
      t.cancel(live[k]);
      live[k] = live.back();
      live.pop_back();
    } else if (r < 33) {
      t.cancel(next_id + 1000);
    } else if (r < 36 && !live.empty()) {
      t.submit({live[pick(0, static_cast<int>(live.size()) - 1)], ob::Side::Buy, 100, 1});
    } else if (r < 38) {
      t.submit({next_id++, ob::Side::Sell, pick(-1, 0), pick(-2, 5)});
    } else {
      const ob::Side side = pick(0, 1) ? ob::Side::Buy : ob::Side::Sell;
      const ob::Price mid = 100;
      const ob::Price px = side == ob::Side::Buy ? mid + pick(-5, 2) : mid + pick(-2, 5);
      const ob::OrderId id = next_id++;
      t.submit({id, side, px, pick(1, 20)});
      if (t.book().contains(id)) live.push_back(id);
    }
  }
}

TEST(differential_random_streams) {
  for (std::uint64_t seed : {1u, 2u, 3u, 42u, 1234u}) run_stream(seed, 4096, 20000);
}

TEST(differential_tiny_capacity) {
  for (std::uint64_t seed : {7u, 8u, 9u}) run_stream(seed, 8, 5000);
}

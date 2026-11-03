#include <set>
#include <vector>

#include "checked_book.hpp"
#include "harness.hpp"
#include "ob/order_book.hpp"
#include "ob/order_pool.hpp"
#include "ob/pricelevel.hpp"

using namespace test;
using ob::Side;
using ob::Status;
using ob::Trade;

TEST(pool_hands_out_distinct_nodes_until_full) {
  ob::OrderPool pool(3);
  std::set<ob::NodeIndex> seen;
  for (int i = 0; i < 3; ++i) {
    CHECK(pool.has_free());
    seen.insert(pool.allocate(i, 10));
  }
  CHECK(seen.size() == 3);
  CHECK(!pool.has_free());
  CHECK(pool.in_use() == 3);
}

TEST(pool_reuses_released_nodes) {
  ob::OrderPool pool(2);
  auto a = pool.allocate(1, 10);
  pool.allocate(2, 10);
  pool.release(a);
  CHECK(pool.in_use() == 1);
  CHECK(pool.allocate(3, 5) == a);
  CHECK(pool[a].id == 3 && pool[a].qty == 5);
  CHECK(pool[a].prev == ob::kNullNode && pool[a].next == ob::kNullNode);
}

TEST(pool_zero_capacity_has_no_free) {
  ob::OrderPool pool(0);
  CHECK(!pool.has_free());
  CHECK(pool.capacity() == 0);
}

static std::vector<ob::OrderId> ids(const ob::PriceLevel& lvl, const ob::OrderPool& pool) {
  std::vector<ob::OrderId> out;
  for (auto i = lvl.front(); i != ob::kNullNode; i = pool[i].next) out.push_back(pool[i].id);
  return out;
}

TEST(level_is_fifo_and_tracks_total) {
  ob::OrderPool pool(4);
  ob::PriceLevel lvl;
  CHECK(lvl.empty());
  lvl.push_back(pool, 1, 5);
  lvl.push_back(pool, 2, 7);
  lvl.push_back(pool, 3, 1);
  CHECK((ids(lvl, pool) == std::vector<ob::OrderId>{1, 2, 3}));
  CHECK(lvl.total_qty() == 13);
}

TEST(level_erase_head_middle_tail) {
  ob::OrderPool pool(4);
  ob::PriceLevel lvl;
  auto a = lvl.push_back(pool, 1, 1);
  auto b = lvl.push_back(pool, 2, 2);
  auto c = lvl.push_back(pool, 3, 3);
  auto d = lvl.push_back(pool, 4, 4);
  lvl.erase(pool, b);
  CHECK((ids(lvl, pool) == std::vector<ob::OrderId>{1, 3, 4}));
  lvl.erase(pool, a);
  CHECK((ids(lvl, pool) == std::vector<ob::OrderId>{3, 4}));
  lvl.erase(pool, d);
  CHECK((ids(lvl, pool) == std::vector<ob::OrderId>{3}));
  CHECK(lvl.total_qty() == 3);
  lvl.erase(pool, c);
  CHECK(lvl.empty());
  CHECK(lvl.total_qty() == 0);
  CHECK(pool.in_use() == 0);
}

TEST(level_reduce_keeps_position) {
  ob::OrderPool pool(2);
  ob::PriceLevel lvl;
  auto a = lvl.push_back(pool, 1, 10);
  lvl.push_back(pool, 2, 10);
  lvl.reduce(pool, a, 4);
  CHECK(lvl.front() == a);
  CHECK(pool[a].qty == 6);
  CHECK(lvl.total_qty() == 16);
}

TEST(same_price_fifo) {
  CheckedBook t;
  CHECK(t.submit(sell(1, 100, 5)) == Status::Accepted);
  CHECK(t.submit(sell(2, 100, 5)) == Status::Accepted);
  CHECK(t.submit(buy(3, 100, 7)) == Status::Accepted);
  CHECK((t.last_trades() == std::vector<Trade>{{1, 3, 100, 5}, {2, 3, 100, 2}}));
  CHECK(t.book().order_qty(2) == 3);
}

TEST(better_price_priority) {
  CheckedBook t;
  t.submit(sell(1, 102, 5));
  t.submit(sell(2, 101, 5));
  t.submit(buy(3, 102, 5));
  CHECK((t.last_trades() == std::vector<Trade>{{2, 3, 101, 5}}));
}

TEST(partial_fill_rests_residual) {
  CheckedBook t;
  t.submit(sell(1, 100, 3));
  t.submit(buy(2, 100, 10));
  CHECK((t.last_trades() == std::vector<Trade>{{1, 2, 100, 3}}));
  CHECK(!t.book().best_ask());
  CHECK(t.book().best_bid() == 100);
  CHECK(t.book().order_qty(2) == 7);
}

TEST(multi_level_sweep) {
  CheckedBook t;
  t.submit(sell(1, 100, 2));
  t.submit(sell(2, 101, 2));
  t.submit(sell(3, 102, 2));
  t.submit(sell(4, 103, 2));
  t.submit(buy(5, 102, 5));
  CHECK((t.last_trades() == std::vector<Trade>{{1, 5, 100, 2}, {2, 5, 101, 2}, {3, 5, 102, 1}}));
  CHECK(t.book().best_ask() == 102);
  CHECK(t.book().qty_at(Side::Sell, 102) == 1);
  CHECK(!t.book().best_bid());
}

TEST(trade_at_maker_price) {
  CheckedBook t;
  t.submit(buy(1, 105, 4));
  t.submit(sell(2, 100, 4));
  CHECK((t.last_trades() == std::vector<Trade>{{1, 2, 105, 4}}));
}

TEST(non_crossing_orders_rest) {
  CheckedBook t;
  t.submit(buy(1, 99, 5));
  t.submit(sell(2, 100, 5));
  CHECK(t.last_trades().empty());
  CHECK(t.book().best_bid() == 99);
  CHECK(t.book().best_ask() == 100);
}

TEST(cancel_head) {
  CheckedBook t;
  t.submit(sell(1, 100, 1));
  t.submit(sell(2, 100, 2));
  t.submit(sell(3, 100, 3));
  CHECK(t.cancel(1) == Status::Accepted);
  CHECK((t.book().queue_at(Side::Sell, 100) == std::vector<ob::OrderId>{2, 3}));
}

TEST(cancel_middle) {
  CheckedBook t;
  t.submit(sell(1, 100, 1));
  t.submit(sell(2, 100, 2));
  t.submit(sell(3, 100, 3));
  CHECK(t.cancel(2) == Status::Accepted);
  CHECK((t.book().queue_at(Side::Sell, 100) == std::vector<ob::OrderId>{1, 3}));
  t.submit(buy(4, 100, 4));
  CHECK((t.last_trades() == std::vector<Trade>{{1, 4, 100, 1}, {3, 4, 100, 3}}));
}

TEST(cancel_tail) {
  CheckedBook t;
  t.submit(buy(1, 100, 1));
  t.submit(buy(2, 100, 2));
  CHECK(t.cancel(2) == Status::Accepted);
  t.submit(buy(3, 100, 3));
  CHECK((t.book().queue_at(Side::Buy, 100) == std::vector<ob::OrderId>{1, 3}));
}

TEST(cancel_last_order_removes_level) {
  CheckedBook t;
  t.submit(buy(1, 100, 5));
  t.submit(buy(2, 99, 5));
  CHECK(t.cancel(1) == Status::Accepted);
  CHECK(t.book().best_bid() == 99);
  CHECK(t.book().bids().size() == 1);
}

TEST(cancel_unknown_and_filled_ids) {
  CheckedBook t;
  CHECK(t.cancel(42) == Status::RejectUnknownId);
  t.submit(sell(1, 100, 5));
  t.submit(buy(2, 100, 5));
  CHECK(t.cancel(1) == Status::RejectUnknownId);
  CHECK(t.cancel(2) == Status::RejectUnknownId);
}

TEST(rejects_zero_quantity) {
  CheckedBook t;
  CHECK(t.submit(buy(1, 100, 0)) == Status::RejectBadQty);
  CHECK(t.submit(buy(1, 100, -3)) == Status::RejectBadQty);
  CHECK(t.book().bids().empty());
}

TEST(rejects_bad_price) {
  CheckedBook t;
  CHECK(t.submit(sell(1, 0, 5)) == Status::RejectBadPrice);
  CHECK(t.submit(sell(1, -1, 5)) == Status::RejectBadPrice);
  CHECK(t.book().asks().empty());
}

TEST(rejects_duplicate_live_id) {
  CheckedBook t;
  t.submit(buy(1, 100, 5));
  CHECK(t.submit(buy(1, 101, 5)) == Status::RejectDuplicateId);
  CHECK(t.submit(sell(1, 100, 5)) == Status::RejectDuplicateId);
  CHECK(t.book().order_qty(1) == 5);
}

TEST(id_reusable_after_fill_or_cancel) {
  CheckedBook t;
  t.submit(buy(1, 100, 5));
  t.cancel(1);
  CHECK(t.submit(buy(1, 100, 5)) == Status::Accepted);
  t.submit(sell(2, 100, 5));
  CHECK(t.submit(sell(1, 101, 5)) == Status::Accepted);
}

TEST(rejects_when_capacity_exhausted) {
  CheckedBook t(2);
  t.submit(buy(1, 100, 1));
  t.submit(buy(2, 99, 1));
  CHECK(t.submit(sell(3, 101, 1)) == Status::RejectCapacity);
  CHECK(t.book().order_count() == 2);
  t.cancel(2);
  CHECK(t.submit(sell(3, 101, 1)) == Status::Accepted);
}

TEST(snapshot_levels_best_first) {
  CheckedBook t;
  t.submit(buy(1, 98, 1));
  t.submit(buy(2, 100, 2));
  t.submit(buy(3, 100, 3));
  t.submit(sell(4, 103, 4));
  t.submit(sell(5, 101, 5));
  CHECK((t.book().bids() == std::vector<ob::LevelView>{{100, 5, 2}, {98, 1, 1}}));
  CHECK((t.book().asks() == std::vector<ob::LevelView>{{101, 5, 1}, {103, 4, 1}}));
  CHECK((t.book().bids(1) == std::vector<ob::LevelView>{{100, 5, 2}}));
}

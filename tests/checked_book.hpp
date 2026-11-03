#pragma once

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ob/order_book.hpp"
#include "reference_book.hpp"

namespace test {
inline ob::NewOrder buy(ob::OrderId id, ob::Price px, ob::Qty qty) { return {id, ob::Side::Buy, px, qty}; }
inline ob::NewOrder sell(ob::OrderId id, ob::Price px, ob::Qty qty) { return {id, ob::Side::Sell, px, qty}; }

class CheckedBook {
 public:
  explicit CheckedBook(std::size_t capacity = 4096) : book_(capacity), ref_(capacity) {}

  ob::Status submit(const ob::NewOrder& o) {
    ++n_;
    trades_.clear();
    std::vector<ob::Trade> ref_trades;
    const auto s = book_.submit(o, trades_);
    const auto s_ref = ref_.submit(o, ref_trades);
    if (s != s_ref) fail("status");
    if (trades_ != ref_trades) fail("trades");
    compare();
    return s;
  }

  ob::Status cancel(ob::OrderId id) {
    ++n_;
    trades_.clear();
    const auto s = book_.cancel(id);
    if (s != ref_.cancel(id)) fail("cancel status");
    compare();
    return s;
  }

  const std::vector<ob::Trade>& last_trades() const { return trades_; }
  const ob::OrderBook& book() const { return book_; }

 private:
  void compare() {
    if (book_.bids() != ref_.bids()) fail("bids");
    if (book_.asks() != ref_.asks()) fail("asks");
    if (book_.order_count() != ref_.order_count()) fail("order count");
    if (!book_.check_invariants()) fail("invariants");
  }

  [[noreturn]] void fail(const char* what) const {
    std::ostringstream os;
    os << "divergence in " << what << " at command " << n_;
    throw std::runtime_error(os.str());
  }

  ob::OrderBook book_;
  ReferenceBook ref_;
  std::vector<ob::Trade> trades_;
  std::size_t n_ = 0;
};

}  // namespace test

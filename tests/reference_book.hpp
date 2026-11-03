#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "ob/order_book.hpp"
#include "ob/types.hpp"

namespace test {
class ReferenceBook {
 public:
  explicit ReferenceBook(std::size_t capacity) : capacity_(capacity) {}

  ob::Status submit(const ob::NewOrder& o, std::vector<ob::Trade>& trades) {
    if (o.qty <= 0) return ob::Status::RejectBadQty;
    if (o.price <= 0) return ob::Status::RejectBadPrice;
    if (find(o.id) != resting_.end()) return ob::Status::RejectDuplicateId;
    if (resting_.size() >= capacity_) return ob::Status::RejectCapacity;

    ob::Qty remaining = o.qty;
    while (remaining > 0) {
      auto best = resting_.end();
      for (auto it = resting_.begin(); it != resting_.end(); ++it) {
        if (it->side == o.side || !crosses(o, it->price)) continue;
        if (best == resting_.end() || better(*it, *best)) best = it;
      }
      if (best == resting_.end()) break;
      const ob::Qty fill = std::min(remaining, best->qty);
      trades.push_back({best->id, o.id, best->price, fill});
      remaining -= fill;
      best->qty -= fill;
      if (best->qty == 0) resting_.erase(best);
    }
    if (remaining > 0) resting_.push_back({o.id, o.side, o.price, remaining, seq_++});
    return ob::Status::Accepted;
  }

  ob::Status cancel(ob::OrderId id) {
    auto it = find(id);
    if (it == resting_.end()) return ob::Status::RejectUnknownId;
    resting_.erase(it);
    return ob::Status::Accepted;
  }

  std::vector<ob::LevelView> bids() const { return levels(ob::Side::Buy); }
  std::vector<ob::LevelView> asks() const { return levels(ob::Side::Sell); }
  std::size_t order_count() const { return resting_.size(); }

 private:
  struct Resting {
    ob::OrderId id;
    ob::Side side;
    ob::Price price;
    ob::Qty qty;
    std::uint64_t seq;
  };

  static bool crosses(const ob::NewOrder& o, ob::Price resting) {
    return o.side == ob::Side::Buy ? resting <= o.price : resting >= o.price;
  }

  static bool better(const Resting& a, const Resting& b) {
    if (a.price != b.price) return a.side == ob::Side::Sell ? a.price < b.price : a.price > b.price;
    return a.seq < b.seq;
  }

  std::vector<Resting>::iterator find(ob::OrderId id) {
    return std::find_if(resting_.begin(), resting_.end(), [&](const Resting& r) { return r.id == id; });
  }

  std::vector<ob::LevelView> levels(ob::Side side) const {
    std::map<ob::Price, ob::LevelView> by_price;
    for (const auto& r : resting_) {
      if (r.side != side) continue;
      auto& v = by_price.try_emplace(r.price, ob::LevelView{r.price, 0, 0}).first->second;
      v.qty += r.qty;
      ++v.orders;
    }
    std::vector<ob::LevelView> out;
    for (const auto& [px, v] : by_price) out.push_back(v);
    if (side == ob::Side::Buy) std::reverse(out.begin(), out.end());
    return out;
  }

  std::size_t capacity_;
  std::uint64_t seq_ = 0;
  std::vector<Resting> resting_;
};

}  // namespace test

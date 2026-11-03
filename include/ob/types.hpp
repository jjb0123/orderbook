#pragma once

#include <cstdint>

namespace ob {

using OrderId = std::uint64_t;
using Price = std::int64_t;  // ticks
using Qty = std::int64_t;    // signed so negative input can be rejected

enum class Side : std::uint8_t { Buy, Sell };

struct NewOrder {
  OrderId id;
  Side side;
  Price price;
  Qty qty;
};

enum class Status : std::uint8_t {
  Accepted,
  RejectBadQty,       // qty <= 0
  RejectBadPrice,     // price <= 0
  RejectDuplicateId,  // id belongs to a resting order
  RejectUnknownId,    // cancel of an id that isn't resting
  RejectCapacity,     // node pool exhausted
};

struct Trade {
  OrderId maker;
  OrderId taker;
  Price price;  // the resting (maker) order's price
  Qty qty;

  friend bool operator==(const Trade&, const Trade&) = default;
};

}  // namespace ob

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "ob/order_pool.hpp"
#include "ob/pricelevel.hpp"
#include "ob/types.hpp"

namespace ob {
    struct LevelView {
        Price price;
        Qty qty;
        std::size_t orders;

        friend bool operator==(const LevelView&, const LevelView&) = default;
    };

    class OrderBook {
        public:
            explicit OrderBook(std::size_t capacity) : pool_(capacity) {
                orders_.reserve(capacity);
            }

            Status submit(const NewOrder& o, std::vector<Trade>& trades) {
                if (o.qty <= 0) return Status::RejectBadQty;
                if (o.price <= 0) return Status::RejectBadPrice;
                if (orders_.contains(o.id)) return Status::RejectDuplicateId;
                if (!pool_.has_free()) return Status::RejectCapacity;

                Qty remaining = o.qty;
                if (o.side == Side::Buy) {
                    remaining = match(asks_, o, remaining, trades,
                                      [&](Price ask) { return ask <= o.price; });
                    if (remaining > 0) rest(bids_, o, remaining);
                } else {
                    remaining = match(bids_, o, remaining, trades,
                                      [&](Price bid) { return bid >= o.price; });
                    if (remaining > 0) rest(asks_, o, remaining);
                }
                return Status::Accepted;
            }

            Status cancel(OrderId id) {
                auto it = orders_.find(id);
                if (it == orders_.end()) return Status::RejectUnknownId;
                const Locator loc = it->second;
                orders_.erase(it);
                if (loc.side == Side::Buy) {
                    remove(bids_, loc);
                } else {
                    remove(asks_, loc);
                }
                return Status::Accepted;
            }

            std::optional<Price> best_bid() const {
                if (bids_.empty()) return std::nullopt;
                return bids_.begin()->first;
            }
            std::optional<Price> best_ask() const {
                if (asks_.empty()) return std::nullopt;
                return asks_.begin()->first;
            }

            Qty qty_at(Side side, Price price) const {
                return side == Side::Buy ? level_qty(bids_, price) : level_qty(asks_, price);
            }

            bool contains(OrderId id) const {
                return orders_.contains(id);
            }
            Qty order_qty(OrderId id) const {
                auto it = orders_.find(id);
                return it == orders_.end() ? 0 : pool_[it->second.node].qty;
            }
            std::size_t order_count() const {
                return orders_.size();
            }
            std::size_t capacity() const {
                return pool_.capacity();
            }

            std::vector<LevelView> bids(std::size_t depth = SIZE_MAX) const {
                return levels(bids_, depth);
            }
            std::vector<LevelView> asks(std::size_t depth = SIZE_MAX) const {
                return levels(asks_, depth);
            }

            std::vector<OrderId> queue_at(Side side, Price price) const {
                return side == Side::Buy ? ids_at(bids_, price) : ids_at(asks_, price);
            }

            bool check_invariants() const {
                auto bb = best_bid();
                auto ba = best_ask();
                if (bb && ba && *bb >= *ba) return false;
                std::size_t nodes = 0;
                if (!side_ok(bids_, Side::Buy, nodes)) return false;
                if (!side_ok(asks_, Side::Sell, nodes)) return false;
                return nodes == orders_.size() && nodes == pool_.in_use();
            }

        private:
            struct Locator {
                Side side;
                Price price;
                NodeIndex node;
            };

            using Bids = std::map<Price, PriceLevel, std::greater<>>;
            using Asks = std::map<Price, PriceLevel, std::less<>>;

            template <class Levels, class Crosses>
            Qty match(Levels& opp, const NewOrder& o, Qty remaining,
                      std::vector<Trade>& trades, Crosses crosses) {
                while (remaining > 0 && !opp.empty()) {
                    auto lvl_it = opp.begin();
                    const Price px = lvl_it->first;
                    if (!crosses(px)) break;
                    PriceLevel& lvl = lvl_it->second;
                    while (remaining > 0 && !lvl.empty()) {
                        const NodeIndex h = lvl.front();
                        const Node& maker = pool_[h];
                        const Qty fill = std::min(remaining, maker.qty);
                        trades.push_back(Trade{maker.id, o.id, px, fill});
                        remaining -= fill;
                        if (fill == maker.qty) {
                            orders_.erase(maker.id);
                            lvl.erase(pool_, h);
                        } else {
                            lvl.reduce(pool_, h, fill);
                        }
                    }
                    if (lvl.empty()) opp.erase(lvl_it);
                }
                return remaining;
            }

            template <class Levels>
            void rest(Levels& same, const NewOrder& o, Qty remaining) {
                PriceLevel& lvl = same[o.price];
                const NodeIndex i = lvl.push_back(pool_, o.id, remaining);
                orders_.emplace(o.id, Locator{o.side, o.price, i});
            }

            template <class Levels>
            void remove(Levels& same, const Locator& loc) {
                auto lvl_it = same.find(loc.price);
                assert(lvl_it != same.end());
                lvl_it->second.erase(pool_, loc.node);
                if (lvl_it->second.empty()) same.erase(lvl_it);
            }

            template <class Levels>
            static Qty level_qty(const Levels& side, Price price) {
                auto it = side.find(price);
                return it == side.end() ? 0 : it->second.total_qty();
            }

            template <class Levels>
            std::vector<LevelView> levels(const Levels& side, std::size_t depth) const {
                std::vector<LevelView> out;
                for (const auto& [px, lvl] : side) {
                    if (out.size() == depth) break;
                    std::size_t n = 0;
                    for (NodeIndex i = lvl.front(); i != kNullNode; i = pool_[i].next) ++n;
                    out.push_back(LevelView{px, lvl.total_qty(), n});
                }
                return out;
            }

            template <class Levels>
            std::vector<OrderId> ids_at(const Levels& side, Price price) const {
                std::vector<OrderId> out;
                auto it = side.find(price);
                if (it == side.end()) return out;
                for (NodeIndex i = it->second.front(); i != kNullNode; i = pool_[i].next) {
                    out.push_back(pool_[i].id);
                }
                return out;
            }

            template <class Levels>
            bool side_ok(const Levels& side, Side s, std::size_t& nodes) const {
                for (const auto& [px, lvl] : side) {
                    if (lvl.empty()) return false;
                    Qty sum = 0;
                    NodeIndex prev = kNullNode;
                    for (NodeIndex i = lvl.front(); i != kNullNode; i = pool_[i].next) {
                        const Node& n = pool_[i];
                        if (n.qty <= 0 || n.prev != prev) return false;
                        auto it = orders_.find(n.id);
                        if (it == orders_.end()) return false;
                        const Locator& loc = it->second;
                        if (loc.node != i || loc.side != s || loc.price != px) return false;
                        sum += n.qty;
                        prev = i;
                        if (++nodes > pool_.capacity()) return false;
                    }
                    if (sum != lvl.total_qty()) return false;
                }
                return true;
            }

            OrderPool pool_;
            Bids bids_;
            Asks asks_;
            std::unordered_map<OrderId, Locator> orders_;
    };

} // namespace ob

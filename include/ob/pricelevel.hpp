#pragma once

#include <cassert>
#include "ob/order_pool.hpp"

namespace ob {
    class PriceLevel {
        public:
            bool empty() const {
                return head_ == kNullNode;
            }
            Qty total_qty() const {
                return total_qty_;
            }
            NodeIndex front() const {
                return head_;
            }

            NodeIndex push_back(OrderPool& pool, OrderId id, Qty qty) {
                assert(qty > 0);
                NodeIndex i = pool.allocate(id, qty);
                pool[i].prev = tail_;
                if (tail_ != kNullNode) {
                    pool[tail_].next = i;
                } else {
                    head_ = i;
                }
                tail_ = i;
                total_qty_ += qty;
                return i;
            }

            void reduce(OrderPool& pool, NodeIndex i, Qty by) {
                assert(by > 0 && by < pool[i].qty);
                pool[i].qty -= by;
                total_qty_ -= by;
            }

            void erase(OrderPool& pool, NodeIndex i) {
                Node& n = pool[i];
                if (n.prev != kNullNode) {
                    pool[n.prev].next = n.next;
                } else {
                    head_ = n.next;
                }
                if (n.next != kNullNode) {
                    pool[n.next].prev = n.prev;
                } else {
                    tail_ = n.prev;
                }
                total_qty_ -= n.qty;
                pool.release(i);
            }

        private:
            NodeIndex head_ = kNullNode;
            NodeIndex tail_ = kNullNode;
            Qty total_qty_ = 0;
    };

} // namespace ob

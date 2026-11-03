#pragma once
#include <cstdint>
#include <vector>
#include <cassert>

#include "ob/types.hpp" 

namespace ob {
    using NodeIndex = std::uint32_t;
    inline constexpr NodeIndex kNullNode = UINT32_MAX;
    
    struct Node {
        OrderId id;
        Qty qty;
        NodeIndex prev;
        NodeIndex next;
    };

    class OrderPool {
        public: 
            explicit OrderPool(std::size_t capacity) : nodes_(capacity),
                                                       free_head_(capacity == 0 ? kNullNode : 0),
                                                       in_use_(0) {
                assert(capacity < kNullNode);
                for (size_t i = 0; i < capacity; ++i) {
                    nodes_[i].prev = kNullNode;
                    nodes_[i].next = (i + 1 < capacity) ? static_cast<NodeIndex>(i + 1) : kNullNode;
                }
            };

            NodeIndex allocate(OrderId id, Qty qty) {
                assert(has_free());
                NodeIndex i = free_head_;
                free_head_ = nodes_[i].next;
                nodes_[i].id = id;
                nodes_[i].qty = qty;
                nodes_[i].prev = kNullNode;
                nodes_[i].next = kNullNode;
                ++in_use_;
                return i; // the node that was allocated 
            };

            void release(NodeIndex i) {
                assert(in_use_ != 0);
                nodes_[i].next = free_head_;
                free_head_ = i;
                --in_use_;
            };

            bool has_free() const {
                return (free_head_ != kNullNode);
            };
            size_t capacity() const {
                return nodes_.size();
            };
            size_t in_use() const {
                return in_use_;
            };
            
            Node& operator[](NodeIndex i) {
                return nodes_[i];
            }
            const Node& operator[](NodeIndex i) const {
                return nodes_[i];
            }

        private:
            std::vector<Node> nodes_;
            NodeIndex free_head_;
            size_t in_use_; // number of nodes holding a lvie order
    };




}
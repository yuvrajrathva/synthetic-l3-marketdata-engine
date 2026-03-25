#include "utils/shadow_book.hpp"
#include <stdexcept>

int64_t ShadowBook::apply(L2Action action, Price price, Qty new_qty)
{
    switch (action)
    {
        case L2Action::New:
        {
            auto& level = levels_[price];
            int64_t delta = static_cast<int64_t>(new_qty) - static_cast<int64_t>(level.total_qty);
            level.total_qty = new_qty;
            return delta;
        }
        case L2Action::Change:
        {
            auto it = levels_.find(price);
            if (it == levels_.end())
            {
                // treat as New
                auto& level = levels_[price];
                level.total_qty = new_qty;
                return static_cast<int64_t>(new_qty);
            }
            int64_t delta = static_cast<int64_t>(new_qty) - static_cast<int64_t>(it->second.total_qty);
            it->second.total_qty = new_qty;
            return delta;
        }
        case L2Action::Delete:
        {
            auto it = levels_.find(price);
            if (it == levels_.end()) return 0;
            int64_t delta = -static_cast<int64_t>(it->second.total_qty);
            levels_.erase(it);
            return delta;
        }
    }
    return 0;
}

void ShadowBook::consume(Price price, Qty qty, L3EventType type,
                         Timestamp ts, uint64_t recv_ts, uint64_t trade_id,
                         std::vector<L3Event>& out)
{
    auto it = levels_.find(price);
    if (it == levels_.end()) return;

    auto& level  = it->second;
    Qty remaining = qty;

    while (remaining > 0 && !level.orders.empty())
    {
        auto& front = level.orders.front();

        if (front.qty <= remaining)
        {
            // Fully consume this synthetic order
            out.push_back(L3Event{
                .type        = type,
                .side        = {},
                .order_id    = front.order_id,
                .price       = price,
                .qty         = front.qty,
                .exchange_ts = ts,
                .recv_ts_ns  = recv_ts,
                .trade_id    = (type == L3EventType::OrderFill) ? trade_id : 0
            });
            remaining -= front.qty;
            level.orders.pop_front();
        }
        else
        {
            // Partially consume this synthetic order
            out.push_back(L3Event{
                .type        = type,
                .side        = {},
                .order_id    = front.order_id,
                .price       = price,
                .qty         = remaining,
                .exchange_ts = ts,
                .recv_ts_ns  = recv_ts,
                .trade_id    = (type == L3EventType::OrderFill) ? trade_id : 0
            });
            front.qty -= remaining;
            remaining  = 0;
        }
    }

    // Update total_qty
    level.total_qty -= (qty - remaining);
    if (level.orders.empty()) levels_.erase(it);
}

OrderId ShadowBook::add_order(Price price, Qty qty)
{
    auto& level   = levels_[price];
    OrderId id    = next_order_id_++;
    level.orders.push_back({id, qty});
    level.total_qty += qty;
    return id;
}

Qty ShadowBook::get_qty(Price price) const
{
    auto it = levels_.find(price);
    return (it != levels_.end()) ? it->second.total_qty : 0;
}

bool ShadowBook::has_level(Price price) const
{
    return levels_.count(price) > 0;
}
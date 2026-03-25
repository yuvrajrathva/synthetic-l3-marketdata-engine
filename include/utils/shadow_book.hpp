#pragma once

#include <types.hpp>

#include <vector>

struct SyntheticOrder
{
    OrderId order_id;
    Qty qty;
};

struct LevelState
{
    Qty total_qty = 0;
    std::deque<SyntheticOrder> orders;
};

class ShadowBook
{
public:
    // apply L2 and return qty delta
    int64_t apply(L2Action action, Price price, Qty new_qty);
    // generate cancel or fill order for given quantity
    void consume(Price price, Qty qty, L3EventType type, Timestamp ts, uint64_t recv_ts, uint64_t trade_id, std::vector<L3Event>& out);
    // push new order for this quantity to queue
    OrderId add_order(Price price, Qty qty);
    Qty get_qty(Price price) const;
    bool has_level(Price price) const;

private:
    std::map<Price, LevelState> levels_;
    uint64_t next_order_id_ = 1;
};
#pragma once

#include <types.hpp>
#include <unordered_map>

struct LiveOrder 
{
    Price     price;
    Qty       qty;
    Side      side;
    Timestamp added_ts;
};

struct TrackerStats
{
    uint64_t total_adds       = 0;
    uint64_t total_cancels    = 0;
    uint64_t total_fills      = 0;

    // anomalies
    uint64_t unknown_order_id = 0;  // cancel/fill ref to order not in map
    uint64_t qty_exceed       = 0;  // cancel/fill qty > resting qty
    uint64_t duplicate_add    = 0;  // OrderAdd for already-live order_id
};

class L3OrderTracker
{
public:
    void on_event(const L3Event& ev);
    void print_stats() const;
    const TrackerStats& stats() const { return stats_; }

private:
    std::unordered_map<OrderId, LiveOrder> live_orders_;
    TrackerStats stats_;

    void handle_add(const L3Event& ev);
    void handle_reduce(const L3Event& ev);
};
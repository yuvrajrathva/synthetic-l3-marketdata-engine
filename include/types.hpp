#pragma once

#include "types.hpp"

#include <map>
#include <iostream>

using Price = uint32_t;
using Qty = uint32_t;

struct L2Book
{
    std::map<Price, Qty, std::greater<Price>> bids;
    std::map<Price, Qty> asks;

    uint64_t change_id = 0;
    uint64_t prev_change_id = 0;
    uint64_t exchange_ts = 0;
    bool is_synced = false;
};

enum class L2Action : uint8_t
{
    New = 0,
    Change = 1,
    Delete = 2,
};

struct PriceLevelUpdate
{
    Price price;
    Qty qty;
    L2Action action;
    bool is_bid;
};

struct L2Delta
{
    uint64_t exchange_ts_ms;
    uint64_t recv_ts_ns;
    uint64_t change_id;
    uint64_t prev_change_id;
    bool is_snapshot;
    std::array<PriceLevelUpdate, 64> updates;
    uint8_t update_count = 0;

    void add(Price p, Qty q, L2Action a, bool bid) noexcept
    {
        if(update_count < 64) updates[update_count++] = {p, q, a, bid};
    }
};

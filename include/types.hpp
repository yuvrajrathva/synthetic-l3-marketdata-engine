#pragma once

#include <map>
#include <iostream>
#include <deque>

using Price = uint64_t; // will do price * 10 to handle 0.5 increments
using Qty = uint32_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;

constexpr Price   INVALID_PRICE    = -1;
constexpr OrderId INVALID_ORDER_ID = 0;

enum class Side : uint8_t 
{
    Bid = 0,
    Ask = 1
};

enum class L2Action : uint8_t
{
    New = 0,
    Change = 1,
    Delete = 2,
};

enum class L3EventType : uint8_t
{
    OrderAdd = 0,
    OrderCancel = 1,
    OrderFill = 2
};

struct L3Event
{
    L3EventType type;
    Side side;
    OrderId order_id;
    Price price;
    Qty qty;
    Timestamp exchange_ts;
    uint64_t recv_ts_ns;
    uint64_t trade_id;
};

struct L2Book
{
    std::map<Price, Qty, std::greater<Price>> bids;
    std::map<Price, Qty> asks;

    uint64_t change_id = 0;
    uint64_t prev_change_id = 0;
    uint64_t exchange_ts = 0;
    bool is_synced = false;
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

#pragma once

#include <types.hpp>

#include <vector>

struct TradeEntry
{
    Qty qty;
    uint64_t trade_id;
};

class TradeBuffer
{
public:
    // add trade if ts present
    void add(Timestamp ts, Price price, Qty qty, uint64_t trade_id);
    // consume up to qty at ts and return available qty
    Qty consume(Timestamp ts, Price price, Qty qty, uint64_t& out_trade_id);

    // added for logging flushed entries
    struct FlushedEntry
    {
        Timestamp ts;
        Price price;
        Qty qty;
        uint64_t trade_id;
    };

    void flush_before(Timestamp ts, std::vector<FlushedEntry>& flushed);
    bool empty() const;

private:
    std::map<Timestamp, std::map<Price, TradeEntry>> buffer_;
};
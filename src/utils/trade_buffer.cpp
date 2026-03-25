#include "utils/trade_buffer.hpp"

void TradeBuffer::add(Timestamp ts, Price price, Qty qty, uint64_t trade_id)
{
    auto& price_map = buffer_[ts];
    auto  it        = price_map.find(price);
    if (it == price_map.end()) price_map[price] = {qty, trade_id};
    else
    {
        it->second.qty      += qty;
        it->second.trade_id  = trade_id;
    }
}

Qty TradeBuffer::consume(Timestamp ts, Price price, Qty qty, uint64_t& out_trade_id)
{
    auto ts_it = buffer_.find(ts);
    if (ts_it == buffer_.end()) return 0;

    auto& price_map = ts_it->second;
    auto  price_it  = price_map.find(price);
    if (price_it == price_map.end()) return 0;

    auto& entry      = price_it->second;
    Qty   available  = std::min(entry.qty, qty);
    out_trade_id     = entry.trade_id;
    entry.qty       -= available;

    if (entry.qty == 0) price_map.erase(price_it);
    if (price_map.empty()) buffer_.erase(ts_it);

    return available;
}

void TradeBuffer::flush_before(Timestamp ts, std::vector<FlushedEntry>& flushed)
{
    auto end = buffer_.lower_bound(ts);  // everything strictly before ts
    for (auto it = buffer_.begin(); it != end; ++it)
    {
        for (auto& [price, entry] : it->second)
        {
            flushed.push_back({it->first, price, entry.qty, entry.trade_id});
        }
    }
    buffer_.erase(buffer_.begin(), end);
}

bool TradeBuffer::empty() const
{
    return buffer_.empty();
}
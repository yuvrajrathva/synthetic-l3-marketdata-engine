#pragma once

#include <types.hpp>
#include <utils/shadow_book.hpp>
#include <utils/trade_buffer.hpp>

#include <functional>

class Correlator
{
public:
    using EventCallback = std::function<void(const L3Event&)>;

    void set_event_callback(EventCallback cb);
    
    // called by MDH when trade arrive
    void on_trade(Timestamp ts, Price price, Qty qty, Side aggressor_side, uint64_t trade_id);
    // called by MDH when L2 update arrives
    void on_l2_update(Timestamp ts, L2Action action, Price price, Qty new_qty, Side side, uint64_t recv_ts);
    // call periodically
    void flush_before(Timestamp ts);

private:
    ShadowBook bid_book_;
    ShadowBook ask_book_;
    TradeBuffer trade_buffer_;
    EventCallback event_cb_;

    ShadowBook& book_for(Side s);
    void process_decrease(Timestamp ts, Price price, Qty decrease_qty, Side side, uint64_t recv_ts);
};
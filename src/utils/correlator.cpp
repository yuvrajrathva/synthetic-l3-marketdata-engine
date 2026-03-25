#include "utils/correlator.hpp"
#include <iostream>

void Correlator::set_event_callback(EventCallback cb)
{
    event_cb_ = std::move(cb);
}

ShadowBook& Correlator::book_for(Side s)
{
    return (s == Side::Bid) ? bid_book_ : ask_book_;
}

void Correlator::on_trade(Timestamp ts, Price price, Qty qty,
                          Side aggressor_side, uint64_t trade_id)
{
    trade_buffer_.add(ts, price, qty, trade_id);
}

void Correlator::on_l2_update(Timestamp ts, L2Action action,
                               Price price, Qty new_qty,
                               Side side, uint64_t recv_ts)
{
    flush_before(ts);

    auto& book  = book_for(side);
    int64_t delta = book.apply(action, price, new_qty);

    if (delta > 0)
    {
        // new OrderAdd immediately
        OrderId id = book.add_order(price, static_cast<Qty>(delta));
        if (event_cb_)
        {
            event_cb_(L3Event{
                .type        = L3EventType::OrderAdd,
                .side        = side,
                .order_id    = id,
                .price       = price,
                .qty         = static_cast<Qty>(delta),
                .exchange_ts = ts,
                .recv_ts_ns  = static_cast<uint64_t>(recv_ts),
                .trade_id    = 0
            });
        }
    }
    else if (delta < 0)
    {
        Qty decrease = static_cast<Qty>(-delta);
        process_decrease(ts, price, decrease, side, recv_ts);
    }
}

void Correlator::process_decrease(Timestamp ts, Price price, Qty decrease_qty,
                                   Side side, uint64_t recv_ts)
{
    uint64_t trade_id    = 0;
    Qty      filled_qty  = trade_buffer_.consume(ts, price, decrease_qty, trade_id);
    Qty      cancel_qty  = decrease_qty - filled_qty;

    auto& book = book_for(side);

    std::vector<L3Event> events;

    if (filled_qty > 0)
        book.consume(price, filled_qty, L3EventType::OrderFill,
                     ts, recv_ts, trade_id, events);

    if (cancel_qty > 0)
        book.consume(price, cancel_qty, L3EventType::OrderCancel,
                     ts, recv_ts, 0, events);

    // Set side on all emitted events (consume() doesn't know side)
    if (event_cb_)
    {
        for (auto& ev : events)
        {
            ev.side = side;
            event_cb_(ev);
        }
    }
}

void Correlator::flush_before(Timestamp ts)
{
    std::vector<TradeBuffer::FlushedEntry> flushed;
    trade_buffer_.flush_before(ts, flushed);

    for (auto& entry : flushed)
    {
        // Trade arrived but L2 never came
        std::cerr << "[ANOMALY:TradeNoL2Match]"
                  << " ts=" << entry.ts
                  << " price=" << entry.price
                  << " qty=" << entry.qty
                  << " trade_id=" << entry.trade_id
                  << "\n";
    }
}
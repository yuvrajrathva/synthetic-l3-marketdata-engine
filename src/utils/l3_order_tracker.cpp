#include <utils/l3_order_tracker.hpp>

void L3OrderTracker::on_event(const L3Event& ev)
{
    switch(ev.type)
    {
        case L3EventType::OrderAdd: handle_add(ev); break;
        case L3EventType::OrderCancel: handle_reduce(ev); break;
        case L3EventType::OrderFill: handle_reduce(ev); break;
    }
};

void L3OrderTracker::handle_add(const L3Event& ev)
{
    auto [it, inserted] = live_orders_.emplace(ev.order_id, LiveOrder{
        .price = ev.price,
        .qty = ev.qty,
        .side = ev.side,
        .added_ts = ev.exchange_ts
    });

    if(!inserted)
    {
        ++stats_.duplicate_add;
        std::cerr << "[TRACKER:DuplicateAdd]"
                  << " order_id=" << ev.order_id
                  << " price="    << ev.price
                  << " side="     << (ev.side == Side::Bid ? "bid" : "ask")
                  << " ts="       << ev.exchange_ts
                  << "\n";
        return;
    }

    ++stats_.total_adds;
}

void L3OrderTracker::handle_reduce(const L3Event& ev)
{
    auto it = live_orders_.find(ev.order_id);
    if(it == live_orders_.end())
    {
        ++stats_.unknown_order_id;
        std::cerr << "[TRACKER:UnknownOrderId]"
                  << " type="     << (ev.type == L3EventType::OrderCancel ? "cancel" : "fill")
                  << " order_id=" << ev.order_id
                  << " price="    << ev.price
                  << " qty="      << ev.qty
                  << " ts="       << ev.exchange_ts
                  << "\n";
        return;
    }

    auto& order = it->second;
    if(order.price != ev.price)
    {
        std::cerr << "[TRACKER:PriceMismatch]"
                  << " order_id="     << ev.order_id
                  << " stored_price=" << order.price
                  << " event_price="  << ev.price
                  << "\n";
    }

    if (ev.qty > order.qty)
    {
        ++stats_.qty_exceed;
        std::cerr << "[TRACKER:QtyExceed]"
                  << " order_id="   << ev.order_id
                  << " resting="    << order.qty
                  << " reduce_by="  << ev.qty
                  << " price="      << ev.price
                  << " ts="         << ev.exchange_ts
                  << "\n";

        order.qty = 0;
    }
    else
    {
        order.qty -= ev.qty;
    }

    if(ev.type == L3EventType::OrderCancel) ++stats_.total_cancels;
    else ++stats_.total_fills;

    if(order.qty == 0) live_orders_.erase(it);
}

void L3OrderTracker::print_stats() const
{
    std::cout << "\n=== L3OrderTracker Stats ===\n"
              << "  Live orders  : " << live_orders_.size()  << "\n"
              << "  Total adds   : " << stats_.total_adds     << "\n"
              << "  Total cancels: " << stats_.total_cancels  << "\n"
              << "  Total fills  : " << stats_.total_fills    << "\n"
              << "--- Anomalies ---\n"
              << "  Unknown ID   : " << stats_.unknown_order_id << "\n"
              << "  Qty exceed   : " << stats_.qty_exceed        << "\n"
              << "  Duplicate add: " << stats_.duplicate_add     << "\n"
              << "============================\n";
}
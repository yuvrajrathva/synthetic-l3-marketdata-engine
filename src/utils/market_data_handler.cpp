#include "utils/market_data_handler.hpp"
#include "external_utils/simdjson.h"
#include "types.hpp"

#include <cstdio>
#include <iostream>

struct MarketDataHandler::Impl {
    simdjson::ondemand::parser parser;
};

MarketDataHandler::MarketDataHandler()
    : pimpl_(std::make_unique<Impl>())
    , error_cb_(nullptr)
{
    correlator_.set_event_callback([this](const L3Event& ev) {
        tracker_.on_event(ev);

        const char* type_str =
            ev.type == L3EventType::OrderAdd    ? "Add   " :
            ev.type == L3EventType::OrderCancel ? "Cancel" : "Fill  ";

        const char* side_str = ev.side == Side::Bid ? "bid" : "ask";

        std::cout << "[L3] " << type_str
                  << " | " << side_str
                  << " | id="    << ev.order_id
                  << " | price=" << (ev.price / 10.0)
                  << " | qty="   << ev.qty
                  << " | ts="    << ev.exchange_ts
                  << (ev.trade_id ? " | trade_id=" : "")
                  << (ev.trade_id ? std::to_string(ev.trade_id) : "")
                  << "\n";
    });
}

MarketDataHandler::~MarketDataHandler() = default;

void MarketDataHandler::set_error_callback(ErrorCallback cb) noexcept
{
    error_cb_ = std::move(cb);
}

void MarketDataHandler::on_message(std::string_view raw_msg) noexcept
{
    try
    {
        const uint64_t recv_ts = get_timestamp_ns();
        simdjson::padded_string padded(raw_msg);
        auto doc = pimpl_->parser.iterate(padded);

        std::string_view method;
        if (doc["method"].get_string().get(method) != simdjson::SUCCESS) return;
        if (method != "subscription") return;
        
        auto params = doc["params"].get_object();
        if (params.error()) return;
        
        auto data_field = params.value()["data"];
        
        if (auto data = data_field.get_array(); !data.error())
        {
            // Process trades
            for (auto element : data.value())
            {
                auto obj = element.get_object();

                auto ts_res = obj["timestamp"].get_int64();
                auto price_res = obj["price"].get_double();
                auto dir_res = obj["direction"].get_string();
                auto qty_res = obj["amount"].get_double();

                if (ts_res.error() || price_res.error() || dir_res.error() || qty_res.error())
                {
                    std::cerr << "[PARSE ERROR] Skipping trade\n";
                    continue;
                }

                Price price     = to_price(price_res.value());
                Qty   qty       = static_cast<Qty>(qty_res.value());
                Side  agg_side  = (dir_res.value() == "buy") ? Side::Ask : Side::Bid;
                Timestamp ts    = static_cast<Timestamp>(ts_res.value());

                std::string_view trade_id_sv;
                uint64_t trade_id = 0;
                if (obj["trade_id"].get_string().get(trade_id_sv) == simdjson::SUCCESS)
                    trade_id = std::stoull(std::string(trade_id_sv));

                correlator_.on_trade(ts, price, qty,
                                    agg_side, trade_id);

                // std::cout 
                //     << "[TRADE] "
                //     << direction << " | "
                //     << "Price: " << price << " | "
                //     << "Qty: " << amount << " | "
                //     << "Ts: " << timestamp
                //     << "\n";
            }
        }
        else if (auto data = data_field.get_object(); !data.error())
        {
            auto ts_res = data.value()["timestamp"].get_int64();
            if (ts_res.error()) return;
            Timestamp ts    = static_cast<Timestamp>(ts_res.value());

            auto process_levels = [&](simdjson::ondemand::array& arr, Side side)
            {
                for (auto elem : arr)
                {
                    auto level = elem.get_array();
                    if (level.error()) continue;

                    auto it = level.value().begin();

                    std::string_view action_sv;
                    if ((*it).get_string().get(action_sv) != simdjson::SUCCESS) continue;
                    ++it;

                    double price_d;
                    if ((*it).get_double().get(price_d) != simdjson::SUCCESS) continue;
                    ++it;

                    double qty_d;
                    if ((*it).get_double().get(qty_d) != simdjson::SUCCESS) continue;

                    L2Action action;
                    if      (action_sv == "new")    action = L2Action::New;
                    else if (action_sv == "change") action = L2Action::Change;
                    else                            action = L2Action::Delete;

                    Price price   = to_price(price_d);
                    Qty   new_qty = static_cast<Qty>(qty_d);

                    correlator_.on_l2_update(ts, action, price, new_qty, side, recv_ts);
                }
            };

            auto bids = data.value()["bids"].get_array();
            if (!bids.error()) process_levels(bids.value(), Side::Bid);

            auto asks = data.value()["asks"].get_array();
            if (!asks.error()) process_levels(asks.value(), Side::Ask);
        }
        else
        {
            std::cerr << "[ERROR] Unknown data type\n";
        }
    }
    catch (const simdjson::simdjson_error& e)
    {
        if (error_cb_) {
            snprintf(error_buffer_, ERROR_BUF_SIZE, 
                    "JSON error: %s", simdjson::error_message(e.error()));
            error_cb_(error_buffer_);
        }
    }
}

inline uint64_t MarketDataHandler::get_timestamp_ns() const noexcept 
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}
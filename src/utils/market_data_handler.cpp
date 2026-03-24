#include "utils/market_data_handler.hpp"
#include "external_utils/simdjson.h"
#include <cstdio>
#include <iostream>

struct MarketDataHandler::Impl {
    simdjson::ondemand::parser parser;
};

MarketDataHandler::MarketDataHandler()
    : pimpl_(std::make_unique<Impl>())
    , error_cb_(nullptr){}

MarketDataHandler::~MarketDataHandler() = default;

void MarketDataHandler::set_error_callback(ErrorCallback cb) noexcept
{
    error_cb_ = std::move(cb);
}

void MarketDataHandler::on_message(std::string_view raw_msg) noexcept
{
    try
    {
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

                int64_t timestamp = ts_res.value();
                double price = price_res.value();
                std::string_view direction = dir_res.value();
                double amount = qty_res.value();

                std::cout 
                    << "[TRADE] "
                    << direction << " | "
                    << "Price: " << price << " | "
                    << "Qty: " << amount << " | "
                    << "Ts: " << timestamp
                    << "\n";
            }
        }
        else if (auto data = data_field.get_object(); !data.error())
        {
            auto ts_res = data.value()["timestamp"].get_int64();
            if (ts_res.error()) return;
            int64_t timestamp = ts_res.value();

            // Process bids
            auto bids_result = data.value()["bids"].get_array();
            if (!bids_result.error()) {
                for (auto level_elem : bids_result.value()) {
                    auto level_array = level_elem.get_array();
                    if (level_array.error()) continue;
                    
                    auto iter = level_array.value().begin();

                    std::string_view action;
                    if ((*iter).get_string().get(action) != simdjson::SUCCESS) continue;

                    ++iter;
                    double price_double;
                    if ((*iter).get_double().get(price_double) != simdjson::SUCCESS) continue;
                    
                    ++iter;
                    double qty_double;
                    if ((*iter).get_double().get(qty_double) != simdjson::SUCCESS) continue;

                    std::cout 
                    << "[BID] "
                    << action << " | "
                    << "Price: " << price_double << " | "
                    << "Qty: " << qty_double << " | "
                    << "Ts: " << timestamp
                    << "\n";
                }
            }
            
            // Process asks
            auto asks_result = data.value()["asks"].get_array();
            if (!asks_result.error()) {
                for (auto level_elem : asks_result.value()) {
                    auto level_array = level_elem.get_array();
                    if (level_array.error()) continue;
                    
                    auto iter = level_array.value().begin();
                    
                    std::string action;
                    if ((*iter).get_string().get(action) != simdjson::SUCCESS) continue;
                    
                    ++iter;
                    double price_double;
                    if ((*iter).get_double().get(price_double) != simdjson::SUCCESS) continue;
                    
                    ++iter;
                    double qty_double;
                    if ((*iter).get_double().get(qty_double) != simdjson::SUCCESS) continue;

                    std::cout 
                    << "[ASK] "
                    << action << " | "
                    << "Price: " << price_double << " | "
                    << "Qty: " << qty_double << " | "
                    << "Ts: " << timestamp
                    << "\n";
                }
            }
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
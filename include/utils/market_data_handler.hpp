#pragma once

#include <string_view>
#include <functional>
#include <memory>

#include "correlator.hpp"
#include "l3_order_tracker.hpp"
class MarketDataHandler
{
public:
    using ErrorCallback = std::function<void(std::string_view)>;

    explicit MarketDataHandler();
    ~MarketDataHandler();

    MarketDataHandler(const MarketDataHandler&) = delete;
    MarketDataHandler& operator=(const MarketDataHandler&) = delete;

    void on_message(std::string_view raw_msg) noexcept;
    void set_error_callback(ErrorCallback cb) noexcept;
    void print_stats() const { tracker_.print_stats(); }

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    Correlator correlator_;
    L3OrderTracker   tracker_;

    ErrorCallback error_cb_;
    
    static constexpr size_t ERROR_BUF_SIZE = 256;
    char error_buffer_[ERROR_BUF_SIZE];
    
    inline uint64_t get_timestamp_ns() const noexcept;
    static Price to_price(double d) noexcept {
        return static_cast<Price>(d * 10.0);
    }
};
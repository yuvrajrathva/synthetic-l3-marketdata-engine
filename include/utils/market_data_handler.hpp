#pragma once

#include <string_view>
#include <functional>
#include <memory>

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

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    ErrorCallback error_cb_;

    static constexpr size_t ERROR_BUF_SIZE = 256;
    char error_buffer_[ERROR_BUF_SIZE];

    inline uint64_t get_timestamp_ns() const noexcept;
};
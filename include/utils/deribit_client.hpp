#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <memory>

#include <utils/config_loader.hpp>
#include <utils/websocket_client.hpp>

class DeribitClient
{
public:
    using MessageCallback = std::function<void(std::string_view)>;
    using StateCallback = std::function<void(bool connected)>;

    explicit DeribitClient(const ConfigLoader::DeribitConfig& config);
    ~DeribitClient();

    DeribitClient(const DeribitClient&) = delete;
    DeribitClient& operator=(const DeribitClient&) = delete;

    /**
     * @brief Set callback for market data message
     */
    void set_message_callback(MessageCallback callback);

    /**
     * @brief Set callback for connection state changes
     */
    void set_state_callback(StateCallback callback);

    /**
     * @brief Connect to Deribit and authenticate
     */
    bool connect();

    /**
     * @brief Subscribe to configured market data channels
     */
    void subscribe_orderbook(const std::string& instrument);

    /**
     * @brief Run event loop
     */
    void run();

    /**callback
     * @brief Stop the client
     */
    void stop();

    /**
     * @brief Set callback for connection state changes
     */
    bool is_authenticated() const { return authenticated_.load(); }

    /**
     * @brief Set callback for connection state changes
     */
    bool is_connected() const { return ws_client_.is_connected(); }

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    WebSocketClient ws_client_;
    ConfigLoader::DeribitConfig config_;

    std::atomic<bool> authenticated_{false};
    std::atomic<bool> subscribed_{false};

    MessageCallback message_callback_;
    StateCallback state_callback_;

    void setup_callbacks();

    void send_authentication();
    void handle_auth_response(std::string_view message);

    void send_subscription(const std::vector<std::string>& channels);
    void handle_subscription_response(std::string_view message);

    void handle_message(std::string_view message);

    void setup_heartbeat();
    void send_heartbeat();

    std::string build_channel_name(const std::string& channel_template, const std::string& instrument) const;
};
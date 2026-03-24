#pragma once

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

class WebSocketClient
{
public:
    using client_t = websocketpp::client<websocketpp::config::asio_tls_client>;
    using connection_hdl = websocketpp::connection_hdl;
    using message_ptr = client_t::message_ptr;

    using on_message_callback = std::function<void(std::string_view)>;
    using on_open_callback = std::function<void()>;
    using on_close_callback = std::function<void()>;
    using on_error_callback = std::function<void(std::string_view)>;

    enum class ConnectionState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTED,
        FAILED 
    };

    WebSocketClient();
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    bool connect(const std::string& uri);

    void disconnect();

    // send JSON message to server
    bool send(const std::string& message);

    // get current connection state
    ConnectionState get_state() const { return state_.load(); }

    bool is_connected() const { return state_.load() == ConnectionState::CONNECTED; }

    // set callback functions
    void set_on_message(on_message_callback callback) { on_message_ = std::move(callback); }
    void set_on_open(on_open_callback callback) { on_open_ = std::move(callback); }
    void set_on_close(on_close_callback callback) { on_close_ = std::move(callback); }
    void set_on_error(on_error_callback callback) { on_error_ = std::move(callback); }

    void set_reconnect_setting(int max_attempts, std::chrono::milliseconds delay);

    // start client event loop
    void run();

    // start client event loop in background thread
    void run_async();

    // stop event loop
    void stop();

private:
    std::unique_ptr<client_t> client_;
    
    connection_hdl connection_;
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    std::string uri_;

    std::unique_ptr<std::thread> background_thread_;
    std::atomic<bool> should_stop_{false};

    int max_reconnect_attempts_ = 5;
    std::chrono::milliseconds reconnect_delay_{1000};
    std::atomic<int> reconnect_attempts_{0};

    // Callback functions
    on_message_callback on_message_;
    on_open_callback on_open_;
    on_close_callback on_close_;
    on_error_callback on_error_;

    // websocketpp event handlers
    void on_socket_init(connection_hdl hdl);
    void on_open_handler(connection_hdl hdl);
    void on_close_handler(connection_hdl hdl);
    void on_message_handler(connection_hdl hdl, message_ptr msg);
    void on_fail_handler(connection_hdl hdl);

    // reconnection logic
    void handle_reconnect();
    void reset_connection();

    // setup websocketpp client
    void setup_client();
};
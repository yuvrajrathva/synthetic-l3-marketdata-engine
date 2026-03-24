#include "utils/websocket_client.hpp"

WebSocketClient::WebSocketClient()
    : client_(std::make_unique<client_t>())
{
    setup_client();
}

WebSocketClient::~WebSocketClient()
{
    stop();
}

void WebSocketClient::setup_client()
{
    client_->init_asio();

    client_->set_tls_init_handler([](websocketpp::connection_hdl){
        auto ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        try
        {
            ctx->set_options(boost::asio::ssl::context::tlsv12_client |
                            boost::asio::ssl::context::default_workarounds |
                           boost::asio::ssl::context::no_sslv2 |
                           boost::asio::ssl::context::no_sslv3 |
                           boost::asio::ssl::context::single_dh_use);

            ctx->set_default_verify_paths();
            ctx->set_verify_mode(boost::asio::ssl::verify_none);
        }
        catch(const std::exception& e)
        {
            std::cerr << "[TLS ERROR] " << e.what() << '\n';
        }
        return ctx;
    });

    client_-> set_open_handler([this](connection_hdl hdl){
        on_open_handler(hdl);
    });

    client_->set_close_handler([this](connection_hdl hdl){
        on_close_handler(hdl);
    });

    client_->set_message_handler([this](connection_hdl hdl, message_ptr msg){
        on_message_handler(hdl, msg);
    });

    client_->set_fail_handler([this](connection_hdl hdl){
        on_fail_handler(hdl);
    });
}

bool WebSocketClient::connect(const std::string& uri)
{
    if(state_.load() != ConnectionState::DISCONNECTED) return false;

    uri_ = uri;
    state_.store(ConnectionState::CONNECTING);

    try
    {
        websocketpp::lib::error_code ec;
        auto con = client_->get_connection(uri, ec);
        if(ec)
        {
            std::cerr << "[ERROR] Failed to create connection: " << ec.message() << std::endl;
            state_.store(ConnectionState::FAILED);
            return false;
        }

        con->append_header("User-Agent", "HFT-System/1.0");
        con->set_open_handshake_timeout(30000);
        con->set_close_handshake_timeout(5000);

        connection_ = con->get_handle();
        client_->connect(con);
        
        return true;
    }
    catch(const std::exception& e)
    {
        std::cerr << "[ERROR] Connection failed: " << e.what() << std::endl;
        state_.store(ConnectionState::FAILED);
        return false;std::cerr << e.what() << '\n';
    }
}

void WebSocketClient::disconnect()
{
    should_stop_.store(true);
    if(state_.load() == ConnectionState::CONNECTED)
    {
        try
        {
            websocketpp::lib::error_code ec;
            client_->close(connection_, websocketpp::close::status::going_away, "Client disconnect", ec);
            if(ec) std::cerr << "[ERROR] Close failed: " << ec.message() << std::endl;
        }
        catch(const std::exception& e)
        {
            std::cerr << "[ERROR] Disconnect failed: " << e.what() << std::endl;
        }
        state_.store(ConnectionState::DISCONNECTED);
    }
}

bool WebSocketClient::send(const std::string& message)
{
    if(state_.load() != ConnectionState::CONNECTED) return false;

    try
    {
        websocketpp::lib::error_code ec;
        client_->send(connection_, message, websocketpp::frame::opcode::text, ec);

        if(ec)
        {
            std::cerr << "[ERROR] Send failed: " << ec.message() << std::endl;
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        std::cerr << "[ERROR] Send exception: " << e.what() << '\n';
        return false;
    }   
}

void WebSocketClient::set_reconnect_setting(int max_attempts, std::chrono::milliseconds delay)
{
    max_reconnect_attempts_ = max_attempts;
    reconnect_delay_ = delay;
}

void WebSocketClient::run()
{
    try
    {
        client_->run();
    }
    catch(const std::exception& e)
    {
        std::cerr << "[ERROR] Client run failed: " << e.what() << '\n';
        state_.store(ConnectionState::FAILED);
    }
    
}

void WebSocketClient::run_async()
{
    background_thread_ = std::make_unique<std::thread>([this](){
        run();
    });
}

void WebSocketClient::stop()
{
    should_stop_.store(true);

    try
    {
        client_->stop();
        if(background_thread_ && background_thread_->joinable())
        {
            background_thread_->join();
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "[ERROR] Stop failed: " << e.what() << std::endl;
    }
    
}

void WebSocketClient::on_open_handler(connection_hdl hdl)
{
    connection_ = hdl;
    state_.store(ConnectionState::CONNECTED);
    reconnect_attempts_.store(0);

    try
    {
        auto con = client_->get_con_from_hdl(hdl);
    }
    catch(const std::exception& e)
    {
        std::cerr << "[WEBSOCKET] Error getting connection info: " << e.what() << std::endl;
    }

    if(on_open_) on_open_();
}

void WebSocketClient::on_close_handler(connection_hdl hdl)
{
    state_.store(ConnectionState::DISCONNECTED);

    if(on_close_) on_close_();
    if(!should_stop_.load() && reconnect_attempts_.load() < max_reconnect_attempts_) handle_reconnect();
}

void WebSocketClient::on_message_handler(connection_hdl hdl, message_ptr msg)
{
    if(on_message_) on_message_(msg->get_payload());
}

void WebSocketClient::on_fail_handler(connection_hdl hdl)
{
    auto con = client_->get_con_from_hdl(hdl);
    std::string error_msg = con->get_ec().message();
    int response_code = con->get_response_code();

    std::cerr << "[WEBSOCKET ERROR] Connection failed" << std::endl;
    std::cerr << "[ERROR CODE] " << con->get_ec() << " - " << error_msg << std::endl;
    std::cerr << "[HTTP STATUS] " << response_code << std::endl;

    if(response_code > 0)
    {
        std::cerr << "[RESPONSE MSG] " << con->get_response_msg() << std::endl;
        std::cerr << "[SERVER] " << con->get_response_header("Server") << std::endl;
    }

    state_.store(ConnectionState::FAILED);

    if(on_error_) on_error_(error_msg);
    if(!should_stop_.load() && reconnect_attempts_.load() < max_reconnect_attempts_) handle_reconnect();
}

void WebSocketClient::handle_reconnect()
{
    if(should_stop_.load()) return;

    int attempts = reconnect_attempts_.fetch_add(1);
    if(attempts >= max_reconnect_attempts_)
    {
        std::cerr << "[ERROR] Max reconnection attempts reached" << std::endl;
        state_.store(ConnectionState::FAILED);
        return;
    }

    state_.store(ConnectionState::RECONNECTED);
    std::cout << "[RECONNECT] Attempting reconnection "<< (attempts + 1) << "/" << max_reconnect_attempts_ << std::endl;

    std::this_thread::sleep_for(reconnect_delay_);

    if(!should_stop_.load())
    {
        reset_connection();
        connect(uri_);
    }
}

void WebSocketClient::reset_connection()
{
    try
    {
        client_->reset();
        setup_client();
    }
    catch(const std::exception& e)
    {
        std::cerr << "[ERROR] Reset failed: " << e.what() << '\n';
    }
    
}
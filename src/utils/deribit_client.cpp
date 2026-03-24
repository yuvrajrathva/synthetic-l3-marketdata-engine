#include <utils/deribit_client.hpp>
#include <external_utils/simdjson.h>


struct DeribitClient::Impl
{
    simdjson::ondemand::parser parser;
    simdjson::ondemand::parser control_parser;

    std::string access_token;
    std::string refresh_token;
    int64_t expires_in = 0;

    std::unique_ptr<std::thread> heartbeat_thread;
    std::atomic<bool> heartbeat_running{false};
};

DeribitClient::DeribitClient(const ConfigLoader::DeribitConfig& config)
    : config_(config)
    , pimpl_(std::make_unique<Impl>())
{
    setup_callbacks();
}

DeribitClient::~DeribitClient()
{
    stop();
}

void DeribitClient::set_message_callback(MessageCallback callback)
{
    message_callback_ = std::move(callback);
}

void DeribitClient::set_state_callback(StateCallback callback)
{
    state_callback_ = std::move(callback);
}

void DeribitClient::setup_callbacks()
{
    ws_client_.set_on_open([this](){
        std::cout<<"[WS] Connected" <<std::endl;
        send_authentication();
    });

    ws_client_.set_on_close([this](){
        std::cout<<"[WS] Disconnected"<<std::endl;
        authenticated_.store(false);
        subscribed_.store(false);
        if(state_callback_) state_callback_(false);
    });

    ws_client_.set_on_error([](std::string_view error){
        std::cerr<<"[WS ERROR] "<<error<<std::endl;
    });

    ws_client_.set_on_message([this](std::string_view msg){
        handle_message(msg);
    });

    ws_client_.set_reconnect_setting(
        config_.max_reconnect_attempts,
        std::chrono::milliseconds(config_.reconnect_delay_ms)
    );
}

bool DeribitClient::connect()
{
    const char* base_url = config_.credentials.use_testnet
        ? "wss://test.deribit.com/ws/api/v2"
        : "wss://www.deribit.com/ws/api/v2";

    std::cout<<"[WS] Conneting to " << base_url <<std::endl;
    
    if(!ws_client_.connect(base_url))
    {
        std::cerr<<"[WS] Failed to initiate connection" << std::endl;
        return false;
    }

    ws_client_.run_async();

    int wait_count = 0;
    while(!ws_client_.is_connected() && wait_count < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    return ws_client_.is_connected();
}

void DeribitClient::send_authentication() 
{
    std::ostringstream oss;
    oss << "{"
        << "\"jsonrpc\":\"2.0\","
        << "\"id\":1,"
        << "\"method\":\"public/auth\","
        << "\"params\":{"
        << "\"grant_type\":\"client_credentials\","
        << "\"client_id\":\"" << config_.credentials.api_key << "\","
        << "\"client_secret\":\"" << config_.credentials.api_secret << "\""
        << "}}";

    if (ws_client_.send(oss.str())) 
    {
        std::cout << "[AUTH] Request sent" << std::endl;
    } 
    else 
    {
        std::cerr << "[AUTH] Failed to send" << std::endl;
    }
}

void DeribitClient::handle_auth_response(std::string_view message) 
{
    try 
    {
        simdjson::padded_string padded(message);
        auto doc = pimpl_->control_parser.iterate(padded);

        auto result = doc["result"].get_object();
        if (result.error()) 
        {
            std::cerr << "[AUTH] Failed - no result field" << std::endl;
            return;
        }

        auto result_obj = result.value();

        auto access_token_result = result_obj["access_token"].get_string();
        if (!access_token_result.error()) 
        {
            pimpl_->access_token = std::string(access_token_result.value());
        }

        auto refresh_token_result = result_obj["refresh_token"].get_string();
        if (!refresh_token_result.error()) 
        {
            pimpl_->refresh_token = std::string(refresh_token_result.value());
        }

        auto expires_in_result = result_obj["expires_in"].get_int64();
        if (!expires_in_result.error()) 
        {
            pimpl_->expires_in = expires_in_result.value();
        }

        authenticated_.store(true);
        std::cout << "[AUTH] Success! Token expires in " << pimpl_->expires_in << "s" << std::endl;

        if (state_callback_) state_callback_(true);

        setup_heartbeat();

        for (const auto& instrument : config_.instruments) 
        {
            std::cout << "[AUTH] Subscribing to instrument: " << instrument << std::endl;
            subscribe_orderbook(instrument);
        }

    } 
    catch (const simdjson::simdjson_error& e) 
    {
        std::cerr << "[AUTH] Parse error: " << simdjson::error_message(e.error()) << std::endl;
    }
}

void DeribitClient::subscribe_orderbook(const std::string& instrument) 
{
    std::vector<std::string> channels;
    
    for (const auto& channel_template : config_.channels) 
    {
        channels.push_back(build_channel_name(channel_template, instrument));
    }
    send_subscription(channels);
}

void DeribitClient::send_subscription(const std::vector<std::string>& channels) 
{
    if (!authenticated_.load()) 
    {
        std::cerr << "[SUB] Not authenticated" << std::endl;
        return;
    }

    std::ostringstream oss;
    oss << "{"
        << "\"jsonrpc\":\"2.0\","
        << "\"id\":42,"
        << "\"method\":\"private/subscribe\","
        << "\"params\":{\"channels\":[";

    for (size_t i = 0; i < channels.size(); ++i) 
    {
        if (i > 0) oss << ",";
        oss << "\"" << channels[i] << "\"";
    }

    oss << "]}}";

    if (ws_client_.send(oss.str())) 
    {
        std::cout << "[SUB] Subscribed to " << channels.size() << " channels" << std::endl;
        for (size_t i = 0; i < channels.size(); ++i) 
        {
            if (i > 0) std::cout << " ,";
            std::cout << channels[i];
        }
        std::cout<<"\n";
    } 
    else 
    {
        std::cerr << "[SUB] Failed to send" << std::endl;
    }
}

void DeribitClient::handle_subscription_response(std::string_view message) 
{
    try 
    {
        simdjson::padded_string padded(message);
        auto doc = pimpl_->control_parser.iterate(padded);

        auto result = doc["result"];
        if (!result.error()) 
        {
            subscribed_.store(true);
            std::cout << "[SUB] Active" << std::endl;
        }
    } 
    catch (const simdjson::simdjson_error& e) 
    {
        std::cerr << "[SUB] Parse error: " << simdjson::error_message(e.error()) << std::endl;
    }
}

void DeribitClient::handle_message(std::string_view message) 
{
    if (message.find("\"method\":\"subscription\"") != std::string_view::npos) 
    {
        if (message_callback_) 
        {
            message_callback_(message);
        }
        return;
    }
    
    try {
        simdjson::padded_string padded(message);
        auto doc = pimpl_->control_parser.iterate(padded);

        auto id_result = doc["id"].get_int64();
        if (id_result.error()) 
        {
            std::cout << "[CLIENT] No ID field found, error: " << simdjson::error_message(id_result.error()) << std::endl;
            return;
        }

        int64_t id = id_result.value();
        if (id == 1 && !authenticated_.load()) {
            // Routing to auth handler
            handle_auth_response(message);
        } else if (id == 42 && !subscribed_.load()) {
            // Routing to subscription handler
            handle_subscription_response(message);
        } else {
            std::cout << "[CLIENT] ID " << id << " not handled (auth=" << authenticated_.load() << ", sub=" << subscribed_.load() << ")" << std::endl;
        }

    } 
    catch (const simdjson::simdjson_error& e) 
    {
        std::cerr << "[CLIENT] JSON parse error: " << simdjson::error_message(e.error()) << std::endl;
    }
}

void DeribitClient::setup_heartbeat() 
{
    pimpl_->heartbeat_running.store(true);
    
    pimpl_->heartbeat_thread = std::make_unique<std::thread>([this]() {
        while (pimpl_->heartbeat_running.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.heartbeat_interval_ms)
            );
            
            if (authenticated_.load()) {
                send_heartbeat();
            }
        }
    });
}

void DeribitClient::send_heartbeat() 
{
    std::string msg = "{\"jsonrpc\":\"2.0\",\"id\":9929,"
                      "\"method\":\"public/set_heartbeat\","
                      "\"params\":{\"interval\":10}}";
    ws_client_.send(msg);
}

std::string DeribitClient::build_channel_name(const std::string& channel_template, const std::string& instrument) const
{
    std::string channel = channel_template;
    size_t pos = channel.find("{instrument}");
    if (pos != std::string::npos) 
    {
        channel.replace(pos, 12, instrument);
    }
    return channel;
}

void DeribitClient::run() 
{
    while (ws_client_.is_connected()) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void DeribitClient::stop() 
{
    pimpl_->heartbeat_running.store(false);
    if (pimpl_->heartbeat_thread && pimpl_->heartbeat_thread->joinable()) pimpl_->heartbeat_thread->join();
    ws_client_.stop();
}





































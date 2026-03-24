#include "utils/config_loader.hpp"
#include "external_utils/simdjson.h"

#include "fstream"
#include "iostream"
#include "sstream"

namespace ConfigLoader
{
ConfigLoader::DeribitConfig load_config(const std::string& config_path)
{
    DeribitConfig config;
    
    std::ifstream config_file(config_path);
    if(!config_file.is_open())
    {
        std::cerr << "[CONFIG ERROR] Cannot open: " << config_path << std::endl;
        return config;
    }

    std::stringstream buffer;
    buffer << config_file.rdbuf();
    std::string config_content = buffer.str();

    simdjson::ondemand::parser parser;

    try
    {
        simdjson::padded_string padded(config_content);
        auto doc = parser.iterate(padded);

        auto deribit = doc["deribit"].get_object();
        if(deribit.error())
        {
            std::cerr << "[CONFIG ERROR] Missing 'deribit' section in config" << std::endl;
            return config;
        }

        bool use_testnet = true;
        auto testnet_result = deribit.value()["use_testnet"].get_bool();
        if(!testnet_result.error())
        {
            use_testnet = testnet_result.value();
        }
        config.credentials.use_testnet = use_testnet;

        const char* env_key = use_testnet ? "testnet" : "mainnet";
        auto env_obj = deribit.value()[env_key].get_object();
        if(!env_obj.error())
        {
            auto api_key = env_obj.value()["api_key"].get_string();
            if(!api_key.error())
            {
                config.credentials.api_key = std::string(api_key.value());
            }

            auto api_secret = env_obj.value()["api_secret"].get_string();
            if(!api_secret.error())
            {
                config.credentials.api_secret = std::string(api_secret.value());
            }
        }

        auto conn_obj = deribit.value()["connection"].get_object();
        if(!conn_obj.error())
        {
            auto heartbeat = conn_obj.value()["heartbeat_interval_ms"].get_int64();
            config.heartbeat_interval_ms = heartbeat.error() ? 10000 : heartbeat.value();

            auto reconnect = conn_obj.value()["reconnect_delay_ms"].get_int64();
            config.reconnect_delay_ms = heartbeat.error() ? 2000 : heartbeat.value();

            auto max_attempts = conn_obj.value()["max_reconnect_attempts"].get_int64();
            config.max_reconnect_attempts = heartbeat.error() ? 5 : heartbeat.value();
        }

        auto subs_obj = deribit.value()["subscriptions"].get_object();
        if(!subs_obj.error())
        {
            auto instruments = subs_obj.value()["instruments"].get_array();
            if(!instruments.error())
            {
                for(auto inst : instruments.value())
                {
                    auto str = inst.get_string();
                    if(!str.error()) config.instruments.emplace_back(str.value());
                }
            }

            auto channels = subs_obj.value()["channels"].get_array();
            if(!channels.error())
            {
                for(auto chann : channels)
                {
                    auto str = chann.get_string();
                    if(!str.error()) config.channels.emplace_back(str.value());
                }
            }
        }
    }
    catch(const simdjson::simdjson_error& e)
    {
        std::cerr << "[CONFIG ERROR]" << simdjson::error_message(e.error()) << '\n';
    }

    std::cout << "[CONFIG] Loaded" << std::endl;
    return config;
}
}
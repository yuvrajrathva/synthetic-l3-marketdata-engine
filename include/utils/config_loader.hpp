#pragma once

#include <string>
#include <vector>

namespace ConfigLoader
{
struct Credentials
{
    std::string api_key;
    std::string api_secret;
    bool use_testnet = true;
};

struct DeribitConfig
{
    Credentials credentials;
    std::vector<std::string> instruments;
    std::vector<std::string> channels;
    int64_t heartbeat_interval_ms = 10000;
    int64_t reconnect_delay_ms = 2000;
    int64_t max_reconnect_attempts = 5;
};

DeribitConfig load_config(const std::string& config_path);
}
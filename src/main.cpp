#include <iostream>

#include "utils/config_loader.hpp"
#include "utils/deribit_client.hpp"
#include "utils/market_data_handler.hpp"

const std::string config_path = "../deribit_config.json";
std::atomic<bool> g_running(true);

void signal_handler(int signal)
{
    std::cout << "\n [SHUTDOWN] Received signal" << signal << std::endl;
    g_running.store(false);
}

int main()
{
    std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);

    auto config = ConfigLoader::load_config(config_path);
    
    MarketDataHandler md_handler;
    md_handler.set_error_callback([](std::string_view err) {
        std::cerr << "[MD ERROR] " << err << std::endl;
    });
    
    DeribitClient client(config);
    client.set_message_callback([&md_handler](std::string_view msg) {
        md_handler.on_message(msg);
    });

    if (!client.connect()) 
    {
        std::cerr << "[ERROR] Failed to connect" << std::endl;
        return 1;
    }

    std::cout << "Press Ctrl + C to exit..." << std::endl;
    while(g_running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    client.stop();
    std::cout << "Shutdown complete" << std::endl;

    return 0;
}
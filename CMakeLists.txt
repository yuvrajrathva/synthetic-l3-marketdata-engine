cmake_minimum_required(VERSION 3.16)

project(marketdata-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(marketdata-engine 
    src/main.cpp
    src/external_utils/simdjson.cpp
    src/utils/config_loader.cpp
    src/utils/deribit_client.cpp
    src/utils/websocket_client.cpp
    src/utils/market_data_handler.cpp
    src/utils/shadow_book.cpp
    src/utils/l3_order_tracker.cpp
    src/utils/trade_buffer.cpp
    src/utils/correlator.cpp
)

target_include_directories(marketdata-engine PRIVATE include)

target_link_libraries(marketdata-engine
    PRIVATE
        Threads::Threads
        OpenSSL::SSL
)

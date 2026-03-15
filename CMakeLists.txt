cmake_minimum_required(VERSION 3.16)

project(marketdata-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(marketdata-engine 
    src/main.cpp
    src/external_utils/simdjson.cpp
    src/utils/config_loader.cpp
)

target_include_directories(marketdata-engine PRIVATE include)

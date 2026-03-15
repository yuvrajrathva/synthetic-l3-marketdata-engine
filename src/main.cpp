#include <iostream>

#include "utils/config_loader.hpp"

const std::string config_path = "../deribit_config.json";

int main()
{
    auto config = ConfigLoader::load_config(config_path);
}
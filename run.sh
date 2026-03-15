#!/bin/bash
cd build
cmake ..
cmake --build . -j

./marketdata-engine

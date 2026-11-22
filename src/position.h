// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include <string>
#include <ctime>

enum class Direction { NONE, LONG, SHORT };

struct Position {
    bool open = false;
    Direction dir = Direction::NONE;
    std::string asset;
    double entry_price = 0;
    double stop_loss = 0;
    double take_profit = 0;
    double size = 1.0;
    time_t entry_time = 0;
    
    void close() {
        open = false;
        dir = Direction::NONE;
        entry_price = stop_loss = take_profit = 0;
    }
};

struct TradeRecord {
    std::string asset;
    std::string direction;
    double entry_price;
    double exit_price;
    double stop_loss;
    double take_profit;
    double pnl;
    std::string entry_time;
    std::string exit_time;
    std::string result; // "WIN" or "LOSS"
    std::string timeframe;
};

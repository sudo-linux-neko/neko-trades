// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include "position.h"
#include <fstream>
#include <mutex>
#include <iomanip>
#include <sstream>

class CSVLogger {
    std::ofstream file_;
    std::mutex mtx_;
    std::string path_;
public:
    explicit CSVLogger(const std::string& path) : path_(path) {
        file_.open(path, std::ios::app);
        if (file_.tellp() == 0) {
            file_ << "Asset,Direction,Entry,Exit,SL,TP,PnL,EntryTime,ExitTime,Result,Timeframe\n";
        }
    }
    
    void log(const TradeRecord& t) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_ << t.asset << ","
              << t.direction << ","
              << std::fixed << std::setprecision(6) << t.entry_price << ","
              << t.exit_price << ","
              << t.stop_loss << ","
              << t.take_profit << ","
              << t.pnl << ","
              << t.entry_time << ","
              << t.exit_time << ","
              << t.result << ","
              << t.timeframe << "\n";
        file_.flush();
    }
    
    ~CSVLogger() { if (file_.is_open()) file_.close(); }
};

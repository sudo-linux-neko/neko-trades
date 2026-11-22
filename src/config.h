// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

struct Config {
    std::string timeframe = "60";
    std::vector<std::string> assets;
    std::vector<std::string> asset_labels;
    int candle_count = 300;
    double risk_reward_ratio = 1.5;
    double atr_multiplier_sl = 1.5;
    int sr_lookback = 50;
    double sr_tolerance_pct = 0.3;
    int poll_interval_sec = 5;
    std::string csv_path = "trades_log.csv";

    // Simple JSON parser for our config
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        
        timeframe = extractString(content, "timeframe");
        candle_count = extractInt(content, "candle_count");
        risk_reward_ratio = extractDouble(content, "risk_reward_ratio");
        atr_multiplier_sl = extractDouble(content, "atr_multiplier_sl");
        sr_lookback = extractInt(content, "support_resistance_lookback");
        sr_tolerance_pct = extractDouble(content, "sr_tolerance_pct");
        poll_interval_sec = extractInt(content, "poll_interval_sec");
        csv_path = extractString(content, "csv_path");
        assets = extractArray(content, "assets");
        asset_labels = extractArray(content, "asset_labels");
        return true;
    }

private:
    std::string extractString(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        auto start = json.find('"', pos + 1);
        auto end = json.find('"', start + 1);
        return json.substr(start + 1, end - start - 1);
    }
    int extractInt(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos = json.find(':', pos);
        return std::stoi(json.substr(pos + 1));
    }
    double extractDouble(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return 0.0;
        pos = json.find(':', pos);
        return std::stod(json.substr(pos + 1));
    }
    std::vector<std::string> extractArray(const std::string& json, const std::string& key) {
        std::vector<std::string> result;
        std::string search = "\"" + key + "\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return result;
        auto start = json.find('[', pos);
        auto end = json.find(']', start);
        std::string arr = json.substr(start + 1, end - start - 1);
        size_t p = 0;
        while ((p = arr.find('"', p)) != std::string::npos) {
            auto e = arr.find('"', p + 1);
            result.push_back(arr.substr(p + 1, e - p - 1));
            p = e + 1;
        }
        return result;
    }
};

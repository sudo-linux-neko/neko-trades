// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include "indicators.h"
#include <string>
#include <vector>
#include <array>
#include <cstdio>
#include <thread>
#include <chrono>

// Strip the {"timeframe": [...]} wrapper if present
inline std::string unwrapJson(const std::string& raw) {
    if (raw.empty()) return "[]";
    
    // If it starts with '[', it's already a flat array
    size_t first = raw.find_first_not_of(" \t\n\r");
    if (first != std::string::npos && raw[first] == '[') return raw;
    
    // If it starts with '{', find the inner array
    if (first != std::string::npos && raw[first] == '{') {
        size_t arr_start = raw.find('[');
        size_t arr_end   = raw.rfind(']');
        if (arr_start != std::string::npos && arr_end != std::string::npos && arr_end > arr_start) {
            return raw.substr(arr_start, arr_end - arr_start + 1);
        }
    }
    return "[]";
}

inline std::vector<Candle> parseCandles(const std::string& raw_json) {
    std::string json = unwrapJson(raw_json);
    std::vector<Candle> candles;
    if (json.empty() || json == "[]") return candles;

    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t obj_end = json.find('}', pos);
        if (obj_end == std::string::npos) break;
        std::string obj = json.substr(pos, obj_end - pos + 1);

        auto extract = [&](const std::string& key) -> double {
            auto kpos = obj.find("\"" + key + "\"");
            if (kpos == std::string::npos) return 0;
            auto cpos = obj.find(':', kpos);
            if (cpos == std::string::npos) return 0;
            try { return std::stod(obj.substr(cpos + 1)); }
            catch (...) { return 0; }
        };

        Candle c{};
        c.time   = extract("time");
        c.open   = extract("open");
        c.high   = extract("high");
        c.low    = extract("low");
        c.close  = extract("close");
        c.volume = extract("volume");

        // Only add valid candles
        if (c.close > 0 && c.time > 0) {
            candles.push_back(c);
        }

        pos = obj_end + 1;
    }
    return candles;
}

inline std::vector<Candle> fetchCandles(const std::string& symbol,
                                         const std::string& timeframe,
                                         int count) {
    std::string cmd = "node bridge/data_fetcher.js \""
                    + symbol + "\" \""
                    + timeframe + "\" "
                    + std::to_string(count)
                    + " 2>/dev/null";

    std::array<char, 65536> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    size_t n;
    while ((n = fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
        result.append(buffer.data(), n);
    }
    int status = pclose(pipe);
    if (status != 0 && result.empty()) return {};

    return parseCandles(result);
}

// Small delay to avoid TradingView rate limiting
inline void rateLimitDelay() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

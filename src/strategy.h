// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include "indicators.h"
#include "position.h"
#include "config.h"
#include <vector>


struct Signal {
    bool triggered = false;
    Direction dir = Direction::NONE;
    double entry = 0;
    double sl = 0;
    double tp = 0;
};

class Strategy {
    Config cfg_;
public:
    explicit Strategy(const Config& cfg) : cfg_(cfg) {}
    
    // Find support/resistance levels
    std::vector<double> findLevels(const std::vector<Candle>& candles, bool support) {
        std::vector<double> levels;
        int n = candles.size();
        int lb = std::min(cfg_.sr_lookback, n);
        
        for (int i = n - lb; i < n - 1; i++) {
            if (i < 1 || i + 1 >= n) continue;
            if (support) {
                if (candles[i].low <= candles[i-1].low && 
                    candles[i].low <= candles[i+1].low) {
                    levels.push_back(candles[i].low);
                }
            } else {
                if (candles[i].high >= candles[i-1].high && 
                    candles[i].high >= candles[i+1].high) {
                    levels.push_back(candles[i].high);
                }
            }
        }
        return levels;
    }
    
    bool nearLevel(double price, const std::vector<double>& levels) {
        double tol = price * cfg_.sr_tolerance_pct / 100.0;
        for (double lvl : levels) {
            if (std::abs(price - lvl) <= tol) return true;
        }
        return false;
    }
    
    Signal evaluate(const std::vector<Candle>& candles, const Position& pos) {
        Signal sig;
        int n = candles.size();
        if (n < 200) return sig; // Need enough data for 200 EMA
        
        // Calculate indicators
        EMA ema200(200);
        MACD macd;
        ATR atr(14);
        
        double prev_macd = 0, prev_signal = 0;
        double cur_macd = 0, cur_signal = 0;
        double ema_val = 0, atr_val = 0;
        
        bool macd_ready = false;
        for (int i = 0; i < n; i++) {
            ema_val = ema200.update(candles[i].close);
            auto m = macd.update(candles[i].close);
            atr_val = atr.update(candles[i].high, candles[i].low, candles[i].close);
            
            if (i == n - 2) { prev_macd = m.macd_line; prev_signal = m.signal_line; }
            if (i == n - 1) { cur_macd = m.macd_line; cur_signal = m.signal_line; macd_ready = m.valid; }
        }
        
        if (!ema200.ready() || !macd_ready) return sig;
        
        double price = candles[n-1].close;
        double next_open = candles[n-1].close; // approximate next open
        
        // Check existing position for SL/TP
        if (pos.open) {
            // Handled in main loop
            return sig;
        }
        
        // Long signal
        bool macd_cross_up = (prev_macd <= prev_signal) && (cur_macd > cur_signal);
        bool macd_cross_down = (prev_macd >= prev_signal) && (cur_macd < cur_signal);
        
        auto supports = findLevels(candles, true);
        auto resistances = findLevels(candles, false);
        
        if (price > ema_val && macd_cross_up && cur_macd < 0 && nearLevel(price, supports)) {
            sig.triggered = true;
            sig.dir = Direction::LONG;
            sig.entry = next_open;
            sig.sl = next_open - cfg_.atr_multiplier_sl * atr_val;
            double risk = next_open - sig.sl;
            sig.tp = next_open + risk * cfg_.risk_reward_ratio;
        }
        // Short signal
        else if (price < ema_val && macd_cross_down && cur_macd > 0 && nearLevel(price, resistances)) {
            sig.triggered = true;
            sig.dir = Direction::SHORT;
            sig.entry = next_open;
            sig.sl = next_open + cfg_.atr_multiplier_sl * atr_val;
            double risk = sig.sl - next_open;
            sig.tp = next_open - risk * cfg_.risk_reward_ratio;
        }
        
        return sig;
    }
};

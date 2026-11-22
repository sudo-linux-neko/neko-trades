// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

struct Candle {
    double time, open, high, low, close, volume;
};

// Optimized incremental EMA
class EMA {
    int period_;
    double alpha_;
    double value_;
    bool initialized_ = false;
    std::vector<double> buffer_;
public:
    explicit EMA(int period) : period_(period), alpha_(2.0 / (period + 1)), value_(0) {}
    
    void reset() { initialized_ = false; value_ = 0; buffer_.clear(); }
    
    double update(double price) {
        if (!initialized_) {
            buffer_.push_back(price);
            if ((int)buffer_.size() == period_) {
                value_ = 0;
                for (double v : buffer_) value_ += v;
                value_ /= period_;
                initialized_ = true;
                buffer_.clear();
                buffer_.shrink_to_fit();
            }
            return value_;
        }
        value_ = alpha_ * price + (1.0 - alpha_) * value_;
        return value_;
    }
    
    double get() const { return value_; }
    bool ready() const { return initialized_; }
};

// MACD (12, 26, 9)
struct MACDResult {
    double macd_line;
    double signal_line;
    double histogram;
    bool valid;
};

class MACD {
    EMA fast_{12};
    EMA slow_{26};
    EMA signal_{9};
    int count_ = 0;
public:
    void reset() { fast_.reset(); slow_.reset(); signal_.reset(); count_ = 0; }
    
    MACDResult update(double price) {
        count_++;
        double f = fast_.update(price);
        double s = slow_.update(price);
        double macd_line = f - s;
        double sig = signal_.update(macd_line);
        return {macd_line, sig, macd_line - sig, count_ >= 35};
    }
};

// ATR for stop loss calculation
class ATR {
    int period_;
    double value_ = 0;
    double prev_close_ = 0;
    int count_ = 0;
    std::vector<double> trs_;
public:
    explicit ATR(int period = 14) : period_(period) {}
    
    void reset() { value_ = 0; prev_close_ = 0; count_ = 0; trs_.clear(); }
    
    double update(double high, double low, double close) {
        count_++;
        double tr;
        if (count_ == 1) {
            tr = high - low;
        } else {
            tr = std::max({high - low, 
                          std::abs(high - prev_close_), 
                          std::abs(low - prev_close_)});
        }
        prev_close_ = close;
        
        if (count_ <= period_) {
            trs_.push_back(tr);
            if (count_ == period_) {
                value_ = 0;
                for (double t : trs_) value_ += t;
                value_ /= period_;
                trs_.clear();
                trs_.shrink_to_fit();
            }
        } else {
            value_ = (value_ * (period_ - 1) + tr) / period_;
        }
        return value_;
    }
    double get() const { return value_; }
};

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

// test_strategy.cpp
// g++ -std=c++20 -O2 -o test_strategy test_strategy.cpp && ./test_strategy

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>
#include <algorithm>
#include <numeric>
#include <cstdlib>

// ─── Structs ───
struct Candle { double time, open, high, low, close, volume; };

class EMA {
    double alpha_, value_ = 0;
    bool init_ = false;
    int period_;
    std::vector<double> buf_;
public:
    explicit EMA(int p) : period_(p), alpha_(2.0/(p+1)) {}
    void reset() { init_ = false; value_ = 0; buf_.clear(); }
    double update(double price) {
        if (!init_) {
            buf_.push_back(price);
            if ((int)buf_.size() == period_) {
                value_ = std::accumulate(buf_.begin(), buf_.end(), 0.0) / period_;
                init_ = true; buf_.clear(); buf_.shrink_to_fit();
            }
            return value_;
        }
        value_ = alpha_ * price + (1.0 - alpha_) * value_;
        return value_;
    }
    double get() const { return value_; }
    bool ready() const { return init_; }
};

struct MACDResult { double macd_line, signal_line, histogram; bool valid; };

class MACD {
    EMA fast_{12}, slow_{26}, signal_{9};
    int count_ = 0;
public:
    void reset() { fast_.reset(); slow_.reset(); signal_.reset(); count_ = 0; }
    MACDResult update(double price) {
        count_++;
        double f = fast_.update(price);
        double s = slow_.update(price);
        double ml = f - s;
        double sig = signal_.update(ml);
        return {ml, sig, ml - sig, count_ >= 35};
    }
};

class ATR {
    int period_; double value_ = 0, prev_close_ = 0; int count_ = 0;
    std::vector<double> trs_;
public:
    explicit ATR(int p=14) : period_(p) {}
    void reset() { value_ = 0; prev_close_ = 0; count_ = 0; trs_.clear(); }
    double update(double h, double l, double c) {
        count_++;
        double tr = (count_==1) ? h-l : std::max({h-l, std::abs(h-prev_close_), std::abs(l-prev_close_)});
        prev_close_ = c;
        if (count_ <= period_) {
            trs_.push_back(tr);
            if (count_ == period_) {
                value_ = std::accumulate(trs_.begin(), trs_.end(), 0.0) / period_;
                trs_.clear(); trs_.shrink_to_fit();
            }
        } else {
            value_ = (value_*(period_-1)+tr)/period_;
        }
        return value_;
    }
    double get() const { return value_; }
};

enum class Direction { NONE, LONG, SHORT };

// ─── S/R detection ───
std::vector<double> findLevels(const std::vector<Candle>& candles, bool support, int lookback) {
    std::vector<double> levels;
    int n = (int)candles.size();
    int start = std::max(1, n - lookback);
    for (int i = start; i < n - 1; i++) {
        if (support) {
            if (candles[i].low <= candles[i-1].low && candles[i].low <= candles[i+1].low)
                levels.push_back(candles[i].low);
        } else {
            if (candles[i].high >= candles[i-1].high && candles[i].high >= candles[i+1].high)
                levels.push_back(candles[i].high);
        }
    }
    return levels;
}

bool nearLevel(double price, const std::vector<double>& levels, double tol_pct) {
    double tol = price * tol_pct / 100.0;
    for (double lvl : levels) {
        if (std::abs(price - lvl) <= tol) return true;
    }
    return false;
}

// ─── Generate candles that GUARANTEE a long signal on the LAST candle ───
// Pattern: uptrend → pullback (MACD goes negative, price stays > 200EMA) → 
//          bounce off support where MACD crosses above signal while still < 0
std::vector<Candle> generateLongSignalCandles() {
    std::vector<Candle> candles;
    double price = 60000.0;
    int idx = 0;

    // Phase 1: 210 candles strong uptrend (establishes 200 EMA well below price)
    for (int i = 0; i < 210; i++) {
        double change = 30.0 + (rand() % 20) - 5;
        double o = price;
        double c = price + change;
        double h = c + 15 + (rand() % 10);
        double l = o - 10 - (rand() % 10);
        candles.push_back({(double)(idx++), o, h, l, c, 1000});
        price = c;
    }
    // price is now ~66300, 200 EMA is ~63000 (well below)

    // Phase 2: 30 candles pullback (MACD goes negative, price drops but stays > 200 EMA)
    // Create a clear support level here
    double support_level = price - 800;
    for (int i = 0; i < 28; i++) {
        double change = -35.0 + (rand() % 10) - 5;
        double o = price;
        double c = price + change;
        if (c < support_level + 20) c = support_level + 20 + (rand() % 30);
        double h = o + 10;
        double l = std::min(c, support_level + (rand() % 15));
        candles.push_back({(double)(idx++), o, h, l, c, 1200});
        price = c;
    }

    // Phase 3: 1 candle flat at support (establishes the support touch)
    {
        double o = price;
        double c = price - 5;
        double h = o + 8;
        double l = support_level;
        candles.push_back({(double)(idx++), o, h, l, c, 1500});
        price = c;
    }

    // Phase 4: THE signal candle - small bounce, MACD crosses above signal, still < 0
    // We need: prev_macd <= prev_signal AND cur_macd > cur_signal AND cur_macd < 0
    // A small positive candle does this
    {
        double o = price;
        double c = price + 25;  // small bounce
        double h = c + 5;
        double l = o - 3;
        candles.push_back({(double)(idx++), o, h, l, c, 1800});
        price = c;
    }

    return candles;
}

// ─── Scan ALL candles for signals (not just the last) ───
struct SignalHit {
    int index;
    Direction dir;
    double price, ema, macd, signal, atr;
};

std::vector<SignalHit> scanAllSignals(const std::vector<Candle>& candles, double sr_tol) {
    std::vector<SignalHit> hits;
    int n = (int)candles.size();
    if (n < 200) return hits;

    EMA ema200(200);
    MACD macd;
    ATR atr(14);

    double prev_macd = 0, prev_signal = 0;

    for (int i = 0; i < n; i++) {
        double ema_val = ema200.update(candles[i].close);
        auto m = macd.update(candles[i].close);
        double atr_val = atr.update(candles[i].high, candles[i].low, candles[i].close);

        if (i < 200 || !m.valid) { prev_macd = m.macd_line; prev_signal = m.signal_line; continue; }

        double price = candles[i].close;
        bool cross_up   = (prev_macd <= prev_signal) && (m.macd_line > m.signal_line);
        bool cross_down = (prev_macd >= prev_signal) && (m.macd_line < m.signal_line);

        // Use a window of candles for S/R
        std::vector<Candle> window(candles.begin() + std::max(0, i - 50), candles.begin() + i + 1);
        auto supports    = findLevels(window, true, 50);
        auto resistances = findLevels(window, false, 50);

        if (price > ema_val && cross_up && m.macd_line < 0 && nearLevel(price, supports, sr_tol)) {
            hits.push_back({i, Direction::LONG, price, ema_val, m.macd_line, m.signal_line, atr_val});
        }
        if (price < ema_val && cross_down && m.macd_line > 0 && nearLevel(price, resistances, sr_tol)) {
            hits.push_back({i, Direction::SHORT, price, ema_val, m.macd_line, m.signal_line, atr_val});
        }

        prev_macd = m.macd_line;
        prev_signal = m.signal_line;
    }
    return hits;
}

// ─── MAIN ───
int main() {
    srand(42);

    std::cout << "\n";
    std::cout << "  +============================================================+\n";
    std::cout << "  |   NEKO TRADES - STRATEGY TEST BENCH v2                    |\n";
    std::cout << "  +============================================================+\n\n";

    // ═══ TEST 1: Synthetic data with guaranteed signal ═══
    std::cout << "  [TEST 1] Synthetic candles (designed to trigger LONG)\n";
    std::cout << "  " << std::string(60, '-') << "\n";

    auto candles = generateLongSignalCandles();
    int n = (int)candles.size();
    std::cout << "  Generated: " << n << " candles\n";
    std::cout << "  Price: " << std::fixed << std::setprecision(2)
              << candles[0].close << " -> " << candles[n-1].close << "\n\n";

    // Print last 8 candles with indicators
    EMA ema200(200); MACD macd; ATR atr(14);
    double prev_macd = 0, prev_signal = 0;
    double cur_macd = 0, cur_signal = 0;
    double ema_val = 0, atr_val = 0;
    bool macd_valid = false;

    std::cout << "  " << std::setw(4) << "Idx"
              << std::setw(11) << "Close"
              << std::setw(11) << "200EMA"
              << std::setw(10) << "MACD"
              << std::setw(10) << "Signal"
              << std::setw(10) << "Hist"
              << std::setw(9) << "ATR" << "\n";
    std::cout << "  " << std::string(65, '-') << "\n";

    for (int i = 0; i < n; i++) {
        ema_val = ema200.update(candles[i].close);
        auto m = macd.update(candles[i].close);
        atr_val = atr.update(candles[i].high, candles[i].low, candles[i].close);
        if (i == n-2) { prev_macd = m.macd_line; prev_signal = m.signal_line; }
        if (i == n-1) { cur_macd = m.macd_line; cur_signal = m.signal_line; macd_valid = m.valid; }
        if (i >= n - 8) {
            std::cout << "  " << std::setw(4) << i
                      << std::setw(11) << std::setprecision(2) << candles[i].close
                      << std::setw(11) << ema_val
                      << std::setw(10) << m.macd_line
                      << std::setw(10) << m.signal_line
                      << std::setw(10) << m.histogram
                      << std::setw(9) << atr_val << "\n";
        }
    }

    // Check conditions on last candle
    double price = candles[n-1].close;
    auto supports = findLevels(candles, true, 50);
    auto resistances = findLevels(candles, false, 50);

    bool cross_up = (prev_macd <= prev_signal) && (cur_macd > cur_signal);
    bool cross_dn = (prev_macd >= prev_signal) && (cur_macd < cur_signal);

    std::cout << "\n  Last candle check:\n";
    std::cout << "    Price > 200EMA?    " << (price > ema_val ? "\033[32mYES\033[0m" : "\033[31mNO\033[0m")
              << "  (" << std::setprecision(2) << price << " vs " << ema_val << ")\n";
    std::cout << "    MACD cross UP?     " << (cross_up ? "\033[32mYES\033[0m" : "\033[31mNO\033[0m")
              << "  (prevM:" << prev_macd << " prevS:" << prev_signal
              << " curM:" << cur_macd << " curS:" << cur_signal << ")\n";
    std::cout << "    MACD < 0?          " << (cur_macd < 0 ? "\033[32mYES\033[0m" : "\033[31mNO\033[0m")
              << "  (" << cur_macd << ")\n";
    std::cout << "    Near support?      " << (nearLevel(price, supports, 0.8) ? "\033[32mYES\033[0m" : "\033[31mNO\033[0m")
              << "  (" << supports.size() << " levels, tol=0.8%)\n";

    // ═══ TEST 2: Scan ALL candles for any signal ═══
    std::cout << "\n\n  [TEST 2] Scanning ALL " << n << " candles for signals (tol=0.8%)...\n";
    std::cout << "  " << std::string(60, '-') << "\n";

    auto hits = scanAllSignals(candles, 0.8);
    if (hits.empty()) {
        std::cout << "  \033[33m  No signals found in synthetic data.\033[0m\n";
        std::cout << "  Trying wider tolerance (2.0%)...\n";
        hits = scanAllSignals(candles, 2.0);
    }
    if (hits.empty()) {
        std::cout << "  \033[33m  Still no signals. Trying 5.0%...\033[0m\n";
        hits = scanAllSignals(candles, 5.0);
    }

    if (!hits.empty()) {
        std::cout << "  \033[32m  FOUND " << hits.size() << " signal(s)!\033[0m\n\n";
        for (auto& h : hits) {
            std::string dir = (h.dir == Direction::LONG) ? "\033[32mLONG \033[0m" : "\033[31mSHORT\033[0m";
            std::cout << "    Candle #" << h.index << " | " << dir
                      << " | Price:" << std::setprecision(2) << h.price
                      << " | EMA:" << h.ema
                      << " | MACD:" << h.macd
                      << " | Sig:" << h.signal
                      << " | ATR:" << h.atr << "\n";
        }
    } else {
        std::cout << "  \033[31m  No signals even at 5% tolerance.\033[0m\n";
    }

    // ═══ TEST 3: Verify node bridge ═══
    std::cout << "\n\n  [TEST 3] Node.js bridge check\n";
    std::cout << "  " << std::string(60, '-') << "\n";
    std::cout << "  Run this manually:\n";
    std::cout << "    node bridge/data_fetcher.js \"BINANCE:BTCUSDT\" \"60\" 5\n";
    std::cout << "  Expected: JSON array with 5 candle objects\n\n";

    // ═══ TEST 4: Indicator sanity check ═══
    std::cout << "  [TEST 4] Indicator sanity (200 EMA on flat data)\n";
    std::cout << "  " << std::string(60, '-') << "\n";
    {
        EMA test_ema(200);
        for (int i = 0; i < 250; i++) test_ema.update(100.0);
        std::cout << "  200 EMA of constant 100.0 after 250 bars: "
                  << std::setprecision(6) << test_ema.get()
                  << (std::abs(test_ema.get() - 100.0) < 0.001 ? "  \033[32mPASS\033[0m" : "  \033[31mFAIL\033[0m")
                  << "\n";
    }
    {
        EMA test_ema(12);
        for (int i = 0; i < 50; i++) test_ema.update(50.0);
        std::cout << "  12 EMA of constant 50.0 after 50 bars:  "
                  << std::setprecision(6) << test_ema.get()
                  << (std::abs(test_ema.get() - 50.0) < 0.001 ? "  \033[32mPASS\033[0m" : "  \033[31mFAIL\033[0m")
                  << "\n";
    }

    std::cout << "\n  +============================================================+\n";
    std::cout << "  |   Tests complete.                                         |\n";
    std::cout << "  +============================================================+\n\n";

    return 0;
}

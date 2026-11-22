// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#include "config.h"
#include "indicators.h"
#include "strategy.h"
#include "position.h"
#include "csv_logger.h"
#include "terminal_ui.h"
#include "data_bridge.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <unordered_map>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::atomic<bool> g_running{true};
void signalHandler(int) { g_running = false; }

std::string timeNow() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string formatTime(time_t t) {
    if (t == 0) return "";
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Config cfg;
    if (!cfg.load("config.json")) {
        std::cerr << "\033[31m  x Failed to load config.json\033[0m\n";
        return 1;
    }

    std::cout << "\033[32m  + Config loaded | TF=" << cfg.timeframe
              << "m | Assets=" << cfg.assets.size()
              << " | Poll=" << cfg.poll_interval_sec << "s\033[0m\n";

    Strategy strategy(cfg);
    CSVLogger logger(cfg.csv_path);
    TerminalUI ui(cfg);

    struct AssetState {
        Position pos;
        std::vector<Candle> candles;
        std::string last_signal;
        double last_price = 0;
        int fail_count = 0;
    };

    std::unordered_map<std::string, AssetState> states;
    for (auto& a : cfg.assets) states[a] = AssetState{};

    int total_trades = 0, wins = 0, losses = 0;

    std::cout << "\033[36m  > Starting Neko Trades engine...\033[0m\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    while (g_running) {
        ui.clear();
        ui.drawHeader();

        for (size_t i = 0; i < cfg.assets.size() && g_running; i++) {
            const std::string& symbol = cfg.assets[i];
            const std::string& label = (i < cfg.asset_labels.size())
                                        ? cfg.asset_labels[i] : symbol;
            auto& state = states[symbol];

            // Fetch data
            auto new_candles = fetchCandles(symbol, cfg.timeframe, cfg.candle_count);

            if (!new_candles.empty()) {
                state.candles = std::move(new_candles);
                state.last_price = state.candles.back().close;
                state.fail_count = 0;
            } else {
                state.fail_count++;
            }

            if (state.candles.empty()) {
                ui.drawAssetStatus(label, 0, state.pos,
                    "NO DATA (retry " + std::to_string(state.fail_count) + ")");
                rateLimitDelay();
                if (i < cfg.assets.size() - 1) ui.drawSeparator();
                continue;
            }

            // Check SL/TP
            if (state.pos.open) {
                double cur = state.last_price;
                bool hit_sl = false, hit_tp = false;

                if (state.pos.dir == Direction::LONG) {
                    hit_sl = (cur <= state.pos.stop_loss);
                    hit_tp = (cur >= state.pos.take_profit);
                } else {
                    hit_sl = (cur >= state.pos.stop_loss);
                    hit_tp = (cur <= state.pos.take_profit);
                }

                if (hit_sl || hit_tp) {
                    TradeRecord rec;
                    rec.asset = label;
                    rec.direction = (state.pos.dir == Direction::LONG) ? "LONG" : "SHORT";
                    rec.entry_price = state.pos.entry_price;
                    rec.exit_price = cur;
                    rec.stop_loss = state.pos.stop_loss;
                    rec.take_profit = state.pos.take_profit;
                    rec.entry_time = formatTime(state.pos.entry_time);
                    rec.exit_time = timeNow();
                    rec.timeframe = cfg.timeframe;
                    rec.pnl = (state.pos.dir == Direction::LONG)
                            ? cur - state.pos.entry_price
                            : state.pos.entry_price - cur;
                    rec.result = (rec.pnl > 0) ? "WIN" : "LOSS";

                    logger.log(rec);
                    total_trades++;
                    if (rec.pnl > 0) wins++; else losses++;

                    ui.drawTrade(rec);
                    state.pos.close();
                }
            }

            // Check for new signal
            if (!state.pos.open) {
                Signal sig = strategy.evaluate(state.candles, state.pos);
                if (sig.triggered) {
                    state.pos.open = true;
                    state.pos.dir = sig.dir;
                    state.pos.asset = label;
                    state.pos.entry_price = sig.entry;
                    state.pos.stop_loss = sig.sl;
                    state.pos.take_profit = sig.tp;
                    state.pos.entry_time = std::time(nullptr);
                    state.last_signal = (sig.dir == Direction::LONG)
                                        ? "LONG ENTRY" : "SHORT ENTRY";
                } else {
                    state.last_signal = "";
                }
            }

            ui.drawAssetStatus(label, state.last_price, state.pos, state.last_signal);

            // Rate limit: wait between API calls
            rateLimitDelay();

            if (i < cfg.assets.size() - 1) ui.drawSeparator();
        }

        ui.drawFooter(total_trades, wins, losses);

        for (int s = 0; s < cfg.poll_interval_sec && g_running; s++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cout << "\n\033[33m  Neko Trades stopped. Trades saved to "
              << cfg.csv_path << "\033[0m\n";
    return 0;
}

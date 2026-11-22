// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#pragma once
#include "position.h"
#include "config.h"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <chrono>

// ANSI colors
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BG_DARK "\033[48;5;233m"
#define CLEAR   "\033[2J\033[H"

class TerminalUI {
    Config cfg_;
    int frame_ = 0;
public:
    explicit TerminalUI(const Config& cfg) : cfg_(cfg) {}
    
    void clear() { std::cout << CLEAR; }
    
    void drawHeader() {
        frame_++;
        std::string spinner[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        std::string spin = spinner[frame_ % 10];
        
        std::cout << CYAN << BOLD;
        std::cout << "  ╔══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "  ║" << MAGENTA << "  🐱  N E K O   T R A D E S  " << CYAN 
                  << "│ TF: " << YELLOW << cfg_.timeframe << "m" << CYAN
                  << "  │ " << spin << " LIVE" << CYAN << "              ║\n";
        std::cout << "  ╠══════════════════════════════════════════════════════════════════╣\n";
        std::cout << RESET;
    }
    
    void drawAssetStatus(const std::string& label, double price, 
                         const Position& pos, const std::string& signal_str) {
        std::cout << "  ║ ";
        std::cout << BOLD << WHITE << std::left << std::setw(10) << label << RESET << " │ ";
        std::cout << CYAN << std::right << std::setw(12) << std::fixed 
                  << std::setprecision(2) << price << RESET << " │ ";
        
        if (pos.open) {
            std::string dir_str = (pos.dir == Direction::LONG) ? "▲ LONG" : "▼ SHORT";
            std::string col = (pos.dir == Direction::LONG) ? GREEN : RED;
            std::cout << col << BOLD << std::setw(8) << dir_str << RESET << " │ ";
            std::cout << DIM << "SL:" << std::setprecision(2) << pos.stop_loss 
                      << " TP:" << pos.take_profit << RESET;
        } else {
            std::cout << DIM << std::setw(8) << "FLAT" << RESET << " │ ";
            if (!signal_str.empty())
                std::cout << YELLOW << signal_str;
            else
                std::cout << DIM << "waiting...";
        }
        std::cout << RESET << "\n";
    }
    
    void drawTrade(const TradeRecord& t) {
        std::string col = (t.result == "WIN") ? GREEN : RED;
        std::string icon = (t.result == "WIN") ? "✅" : "❌";
        std::cout << "  ║ " << icon << " " << col << BOLD << t.result << RESET 
                  << " │ " << WHITE << t.asset << RESET
                  << " │ " << t.direction
                  << " │ Entry:" << std::fixed << std::setprecision(2) << t.entry_price
                  << " → Exit:" << t.exit_price
                  << " │ PnL: " << col << std::showpos << t.pnl << std::noshowpos << RESET
                  << "\n";
    }
    
    void drawFooter(int total_trades, int wins, int losses) {
        double wr = total_trades > 0 ? (100.0 * wins / total_trades) : 0;
        std::cout << CYAN << BOLD;
        std::cout << "  ╠══════════════════════════════════════════════════════════════════╣\n";
        std::cout << "  ║ Trades: " << WHITE << total_trades 
                  << CYAN << " │ Wins: " << GREEN << wins 
                  << CYAN << " │ Losses: " << RED << losses
                  << CYAN << " │ WR: " << YELLOW << std::fixed << std::setprecision(1) << wr << "%"
                  << CYAN << " │ " << DIM << "Ctrl+C to stop" << RESET << CYAN << "        ║\n";
        std::cout << "  ╚══════════════════════════════════════════════════════════════════╝\n";
        std::cout << RESET;
    }
    
    void drawSeparator() {
        std::cout << CYAN << "  ╠──────────────────────────────────────────────────────────────────╣\n" << RESET;
    }
};

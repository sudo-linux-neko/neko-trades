#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

const TradingView = require('@mathieuc/tradingview');

const symbol    = process.argv[2] || 'BINANCE:BTCUSDT';
const timeframe = process.argv[3] || '5';
const count     = parseInt(process.argv[4]) || 300;

function toTVResolution(tf) {
  const n = parseInt(tf);
  if (n >= 1440) return n === 1440 ? '1D' : n === 10080 ? '1W' : '1D';
  return tf;
}

const client = new TradingView.Client();
const timeout = setTimeout(() => {
  client.end();
  process.stdout.write('[]');
  process.exit(1);
}, 20000);

try {
  const chart = new client.Session.Chart();
  let loaded = false;

  chart.onError((...err) => {
    if (!loaded) {
      loaded = true;
      clearTimeout(timeout);
      process.stderr.write('ERR:' + err.join(' ') + '\n');
      process.stdout.write('[]');
      client.end();
      process.exit(1);
    }
  });

  chart.onUpdate(() => {
    if (loaded) return;
    const raw = chart.periods;
    if (!raw || raw.length < Math.min(count, 10)) return;

    loaded = true;
    clearTimeout(timeout);

    const candles = raw.reverse().map(p => ({
      time:   p.time,
      open:   p.open,
      high:   p.max,
      low:    p.min,
      close:  p.close,
      volume: p.volume || 0
    }));

    process.stdout.write(JSON.stringify(candles));
    client.end();
    process.exit(0);
  });

  chart.setMarket(symbol, {
    timeframe: toTVResolution(timeframe),
    range: count,
  });
} catch (err) {
  clearTimeout(timeout);
  process.stderr.write('ERR:' + err.message + '\n');
  process.stdout.write('[]');
  client.end();
  process.exit(1);
}

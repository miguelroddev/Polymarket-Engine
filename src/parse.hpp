#pragma once

#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <functional>

using i64 = long long;
namespace json = boost::json;

extern std::mutex queueMutex;
extern std::condition_variable queueCV;

i64 parse_fixed(std::string_view s, int decimals);
i64 parse_price_fp(std::string_view s);
i64 parse_size_fp(std::string_view s);
i64 parse_timestamp_ms(std::string_view s);

struct OrderBook {
  std::map<i64, i64, std::greater<>> bids;
  std::map<i64, i64> asks;

  void addBid(i64 price, i64 size);

  void addAsk(i64 price, i64 size);

  void clear();

  i64 best_bid_price() const ;
  i64 best_ask_price() const ;
  i64 best_bid_size()  const ;
  i64 best_ask_size()  const ;
};

struct AssetBook {
  std::string asset_id;
  std::string market;
  i64 tick_size_fp = 0;
  i64 timestamp_ms = 0;
  i64 last_trade_fp = -1;
  OrderBook book;
};


void parser(std::unordered_map<std::string, AssetBook>* books, std::queue<std::string>* toParse);